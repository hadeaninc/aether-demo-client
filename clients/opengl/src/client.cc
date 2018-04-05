/*
   Copyright 2018 Hadean Supercomputing Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#define GL_GLEXT_PROTOTYPES
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "linmath.hh"
#include "statistics.hh"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cassert>
#include <cmath>
#include <deque>
#include <vector>

#include <net.hh>
#include <morton.hh>
#include <repclient.hh>
#include <event.hh>
#include <colour.hh>

struct ui_point {
    vec3f p;
    struct colour c;
    float size;
};

struct worker_info {
    std::vector<ui_point> points;
    client_stats stats;
};

struct statistic {
  double bytes;

  statistic() : bytes(0.0) {
  }

  statistic& operator+=(const statistic& b) {
    bytes += b.bytes;
    return *this;
  }

  statistic& operator/=(const double s) {
    bytes /= s;
    return *this;
  }
};

static std::vector<worker_info> vertices;

enum worker_message_type {
    DEBUG_MSG = 0,
    CLICK_MSG = 1,
};

static std::deque<aether_event_t> click_events;

static void error_callback(int error, const char* description) {
    fprintf(stderr, "glfw error: %s\n", description);
}

static void push_event(aether_event_t event) {
    click_events.push_back(event);
}

static void cursor_callback(GLFWwindow* window, const double x, const double y) {
    aether_event_t event;
    event.type = EVENT_CURSOR_MOVE,
    event.cursor_move.position = {static_cast<float>(x), static_cast<float>(y)};
    push_event(event);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        aether_event_t event;
        event.type = EVENT_MOUSE_CLICK;
        event.mouse_click.button = button;
        event.mouse_click.action = BUTTON_PRESSED;
        event.mouse_click.position = { static_cast<float>(xpos), static_cast<float>(ypos) };
        push_event(event);
    }
}

// http://antongerdelan.net/opengl/raycasting.html
// https://gamedev.stackexchange.com/questions/107902/getting-ray-using-gluunproject-or-inverted-mvp-matrix
static void glhUnProjectf(float winx, float winy, float winwidth, float winheight, mat4x4 view, mat4x4 projection, float *objectCoordinate) {
    // Homogenous clip coords
    vec4 ray_clip;
    ray_clip[0] = 2.0 * winx/winwidth - 1.0;
    ray_clip[1] = 1.0 - 2.0*winy/winheight;
    ray_clip[2] = -1.0;
    ray_clip[3] = 1.0;
    // Eye coords
    mat4x4 inv_projection;
    vec4 ray_eye;
    mat4x4_invert(inv_projection, projection);
    mat4x4_mul_vec4(ray_eye, inv_projection, ray_clip);
    ray_eye[2] = -1.0;
    ray_eye[3] = 0.0;
    // World coords
    mat4x4 inv_view;
    vec4 ray_wor;
    mat4x4_invert(inv_view, view);
    mat4x4_mul_vec4(ray_wor, inv_view, ray_eye);

    objectCoordinate[0]=ray_wor[0];
    objectCoordinate[1]=ray_wor[1];
    objectCoordinate[2]=ray_wor[2];
}

static uint64_t num_workers = 0;
static std::vector<net_tree_cell> cells;

static void process_packet(uint64_t id, struct client_message *message) {
    if (id + 1 > num_workers) {
        cells.resize(id + 1);
        for (uint64_t i = num_workers; i < id + 1; i++) {
            cells[i].code = 0;
            cells[i].level = -1;
        }
        num_workers = id + 1;
        vertices.resize(num_workers);
    }
    vertices[id].stats = message->stats;
    if (message->cell_status == CELL_ALIVE) {
        cells[id] = message->cell;
        vertices[id].points.resize(message->num_points);
        auto &points = vertices[id].points;
        for (uint64_t i = 0; i < message->num_points; ++i) {
            const vec2f position = net_decode_position_2f(message->points[i].net_encoded_position, message->cell);
            points[i].p = { position.x, position.y, 0.0 };
            points[i].size = 64;
            points[i].c = net_decode_color(message->points[i].net_encoded_color);
        }
    } else if (message->cell_status == CELL_DYING) {
        cells[id].code = 0;
        cells[id].level = -1;
    } else {
        assert(0 && "process_packet: unhandled cell status");
    }
}

static void unproject_event(aether_event_t *event, const int width, const int height, mat4x4 view, mat4x4 projection, vec3f camera_pos) {
    assert(event != NULL);
    aether_screen_pos_t *pos = NULL;
    switch (event->type) {
      case EVENT_MOUSE_CLICK:
        pos = &event->mouse_click.position;
        break;
      case EVENT_CURSOR_MOVE:
        pos = &event->cursor_move.position;
        break;
      default:
        break;
    }
    if (pos != NULL) {
        float coords[3] = { 0.0, 0.0, 0.0 };
        glhUnProjectf(pos->x, pos->y, width, height, view, projection, coords);
        assert(std::isfinite(coords[0]) && std::isfinite(coords[1]) && std::isfinite(coords[2]));
        float amount = -camera_pos.z / coords[2];
        float ax = camera_pos.x + amount * coords[0];
        float ay = camera_pos.y + amount * coords[1];
        pos->x = ax;
        pos->y = ay;
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr,
                "usage: %s input_file\n"
                "       %s hostname port\n"
                "       %s hostname port output_file\n",
                argv[0], argv[0], argv[0]);
        exit(1);
    }

    //initialise glfw and window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) exit(1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
    GLFWwindow *window = glfwCreateWindow(800, 600, "demo", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(1);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    glewInit();

    struct repclient_state repstate;

    if (argc == 2)
        repstate = repclient_init_playback(argv[1]);
    else if (argc == 3)
        repstate = repclient_init(argv[1], argv[2]);
    else if (argc == 4)
        repstate = repclient_init_record(argv[1], argv[2], argv[3]);

    //create lines array
    vec3f line_vertices[4] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    };

    //setup the programs and pipeline for points
    #define VERSION "#version 150\n"
    #define QUOTE(...) #__VA_ARGS__
    static const char* point_vertex_shader_text = VERSION QUOTE(
        uniform mat4 mvp;
        in vec3 vcol;
        in vec3 vpos;
        in float vsize;
        out vec3 color;
        out gl_PerVertex {
            vec4 gl_Position;
            float gl_PointSize;
        };
        void main() {
            vec4 pos = mvp * vec4(vpos, 1.0);
            gl_Position = pos;
            gl_PointSize = vsize / pos.z;
            color = vcol;
        }
    );
    static const char* point_fragment_shader_text = VERSION QUOTE(
        in vec3 color;
        out vec4 out_color;
        void main() {
            if (distance(gl_PointCoord, vec2(0.5)) > 0.5)
                discard;
            else
                out_color = vec4(color * 0.8, 1.0);
        }
    );
    char error_log[4096] = {0};
    GLuint program_point_vertex = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &point_vertex_shader_text);
    glGetProgramInfoLog(program_point_vertex, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint program_point_fragment = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &point_fragment_shader_text);
    glGetProgramInfoLog(program_point_fragment, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }

    GLuint pipeline_point = 0;
    glGenProgramPipelines(1, &pipeline_point);
    glUseProgramStages(pipeline_point, GL_VERTEX_SHADER_BIT, program_point_vertex);
    glUseProgramStages(pipeline_point, GL_FRAGMENT_SHADER_BIT, program_point_fragment);

    //setup the programs and pipeline for lines
    static const char* line_vertex_shader_text = VERSION QUOTE(
        uniform mat4 mvp;
        in vec3 vpos;
        out vec4 color;
        out gl_PerVertex {
            vec4 gl_Position;
        };
        void main() {
            vec4 pos = mvp * vec4(vpos, 1.0);
            gl_Position = pos;
            color = vec4(0.5, 0.5, 0.5, 0.5);
        }
    );
    static const char* line_fragment_shader_text = VERSION QUOTE(
        in vec4 color;
        out vec4 out_color;
        void main() {
            out_color = vec4(color);
        }
    );
    GLuint program_line_vertex = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &line_vertex_shader_text);
    glGetProgramInfoLog(program_line_vertex, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint program_line_fragment = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &line_fragment_shader_text);
    glGetProgramInfoLog(program_line_fragment, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint pipeline_line = 0;
    glGenProgramPipelines(1, &pipeline_line);
    glUseProgramStages(pipeline_line, GL_VERTEX_SHADER_BIT, program_line_vertex);
    glUseProgramStages(pipeline_line, GL_FRAGMENT_SHADER_BIT, program_line_fragment);

    //setup vao point
    GLuint vao_point;
    glGenVertexArrays(1, &vao_point);
    glBindVertexArray(vao_point);

    //setup point vertices
    GLuint buffer_point_vertices;
    glGenBuffers(1, &buffer_point_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
    GLint p_mvp_location = glGetUniformLocation(program_point_vertex, "mvp");
    GLint vpos_location = glGetAttribLocation(program_point_vertex, "vpos");
    GLint vcol_location = glGetAttribLocation(program_point_vertex, "vcol");
    GLint vsize_location = glGetAttribLocation(program_point_vertex, "vsize");
    glEnableVertexAttribArray(vpos_location);
    glEnableVertexAttribArray(vcol_location);
    glEnableVertexAttribArray(vsize_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, p));
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, c));
    glVertexAttribPointer(vsize_location, 1, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, size));

    //setup vao line
    GLuint vao_line;
    glGenVertexArrays(1, &vao_line);
    glBindVertexArray(vao_line);

    //setup line vertices
    GLuint buffer_line_vertices;
    glGenBuffers(1, &buffer_line_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_line_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    GLint l_mvp_location = glGetUniformLocation(program_line_vertex, "mvp");
    GLint l_vpos_location = glGetAttribLocation(program_line_vertex, "vpos");
    glEnableVertexAttribArray(l_vpos_location);
    glVertexAttribPointer(l_vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(line_vertices[0]), (void*)offsetof(vec3f, x));

    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_callback);

    glfwSetTime(0);
    vec3f camera_pos = {0, 0, 16};
    statistics<statistic> stats(60.0);
    for (uint64_t frames = 0;; frames++) {
        if (frames % 20 == 0)  {
            const statistic stat = stats.get_sample_per_second(1.0);
            printf("Data in: %f KB/s\n", stat.bytes / 1024.0);
            client_stats client_stats_accum = { 0 };
            for(size_t i = 0; i < vertices.size(); ++i) {
              if (cells[i].level != static_cast<uint64_t>(-1)) {
                printf("Worker %li: num_agents=%lu, num_ghost=%lu\n", i, vertices[i].stats.num_agents, vertices[i].stats.num_agents_ghost);
                client_stats_accum.num_agents += vertices[i].stats.num_agents;
                client_stats_accum.num_agents_ghost += vertices[i].stats.num_agents_ghost;
              }
            }
            printf("Total: num_agents=%lu, num_ghost=%lu\n\n", client_stats_accum.num_agents, client_stats_accum.num_agents_ghost);
        }

        //handle user input
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) || glfwGetKey(window, GLFW_KEY_Q)) {
            glfwSetWindowShouldClose(window, true);
        }
        if (glfwGetKey(window, GLFW_KEY_W))
            camera_pos.z -= 0.1;
        if (glfwGetKey(window, GLFW_KEY_A) || glfwGetKey(window, GLFW_KEY_LEFT))
            camera_pos.x -= 0.1;
        if (glfwGetKey(window, GLFW_KEY_S))
            camera_pos.z += 0.1;
        if (glfwGetKey(window, GLFW_KEY_D) || glfwGetKey(window, GLFW_KEY_RIGHT))
            camera_pos.x += 0.1;
        if (glfwGetKey(window, GLFW_KEY_SPACE) || glfwGetKey(window, GLFW_KEY_UP))
            camera_pos.y += 0.1;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_DOWN))
            camera_pos.y -= 0.1;

        if (glfwWindowShouldClose(window)) {
            glfwDestroyWindow(window);
            return 0;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float) height;
        glViewport(0, 0, width, height);
        double mousex, mousey;
        glfwGetCursorPos(window, &mousex, &mousey);
        mousey = height - mousey;
        mousex = mousex / width - 0.5;
        mousey = mousey / height - 0.5;

        //setup mvp
        mat4x4 model, view, projection, mvp;
        mat4x4_translate(view, -camera_pos.x, -camera_pos.y, -camera_pos.z);
        mat4x4_perspective(projection, 120 * 2 * M_PI / 360, ratio, 0.1, 100);
        //mat4x4_ortho(projection, -ratio * camera_pos.z, ratio * camera_pos.z, -1 * camera_pos.z, 1 * camera_pos.z, 0.01, 100);

        while(!click_events.empty()) {
            aether_event_t *ce = &click_events.front();
            unproject_event(ce, width, height, view, projection, camera_pos);
            repclient_send_message(&repstate, ce, sizeof(*ce));
            click_events.pop_front();
        }

        glClearColor(0.0, 0.0, 0.0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        struct client_message *msg = NULL;
        do {
            uint64_t id;
            size_t msg_size;
            msg = static_cast<struct client_message*>(repclient_tick(&repstate, &id, &msg_size));
            if (msg) {
                process_packet(id, msg);
                statistic stat;
                stat.bytes = msg_size;
                stats += stat;

            }
        } while (msg);

        const bool debug_interaction = false;
        if (debug_interaction && frames % 100 == 0) {
            char str[256];
            int n = snprintf(str, 256, "Interaction test in frame %lu from OpenGL client", frames);
            assert(n < 256);

            std::vector<unsigned char> buf(1+1+strlen(str));
            buf[0] = DEBUG_MSG;
            buf[1] = strlen(str);
            memcpy(&buf[1+1], str, strlen(str));
            repclient_send_message(&repstate, &buf[0], buf.size());
        }

        for (uint64_t i = 0; i < num_workers; i++) {
            if (cells[i].level != (uint64_t)-1) {
                //setup model matrix for lines
                vec2f m_vec = morton_2_decode(cells[i].code);
                mat4x4_identity(model);
                for (int j = 0; j < 3; j++)
                    model[j][j] = 1<<cells[i].level;
                model[3][0] = m_vec.x;
                model[3][1] = m_vec.y;
                model[3][2] = 0.0;
                mat4x4_mul(mvp, projection, view);
                mat4x4_mul(mvp, mvp, model);

                //render cells
                glBindVertexArray(vao_line);
                glBindBuffer(GL_ARRAY_BUFFER, buffer_line_vertices);
                glLineWidth(2);
                glBindProgramPipeline(pipeline_line);
                glProgramUniformMatrix4fv(program_line_vertex, l_mvp_location, 1, GL_FALSE, (const GLfloat*)mvp);
                glDrawArrays(GL_TRIANGLE_FAN, 0, sizeof(line_vertices) / sizeof(line_vertices[0]));
                glDrawArrays(GL_LINE_LOOP, 0, sizeof(line_vertices) / sizeof(line_vertices[0]));
            }
        }
        for (uint64_t i = 0; i < num_workers; i++) {
            if (cells[i].level != (uint64_t)-1) {
                //setup mvp matrix for entities
                mat4x4_mul(mvp, projection, view);

                //render entities
                glBindVertexArray(vao_point);
                glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
                const size_t num_points = vertices[i].points.size();
                glBufferData(GL_ARRAY_BUFFER, sizeof(struct ui_point) * num_points, &vertices[i].points[0], GL_DYNAMIC_DRAW);
                glBindProgramPipeline(pipeline_point);
                glProgramUniformMatrix4fv(program_point_vertex, p_mvp_location, 1, GL_FALSE, (const GLfloat*)mvp);
                glDrawArrays(GL_POINTS, 0, num_points);
            }
        }

        glfwPollEvents();
        glfwSwapBuffers(window);
    }
}
