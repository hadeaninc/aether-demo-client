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

#include <gdnative_api_struct.gen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <repclient.hh>
#include <tcp.hh>

extern "C" {

const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;

void GDN_EXPORT godot_gdnative_init(godot_gdnative_init_options *p_options) {
    api = p_options->api_struct;

    // now find our extensions
    for (unsigned int i = 0; i < api->num_extensions; i++) {
        switch (api->extensions[i]->type) {
            case GDNATIVE_EXT_NATIVESCRIPT: {
                nativescript_api = (godot_gdnative_ext_nativescript_api_struct *)api->extensions[i];
            }; break;
            default: break;
        };
    };
}

void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *p_options) {
    api = NULL;
    nativescript_api = NULL;
}

GDCALLINGCONV void *aether_repclient_constructor(godot_object *p_instance, void *p_method_data) {
    printf("AetherRepClient._init()\n");
    auto ret = (repclient_state *) api->godot_alloc(sizeof(struct repclient_state));
    ret->sockfd = -1;
    return ret;
}

GDCALLINGCONV void aether_repclient_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data) {
    printf("AetherRepClient._byebye()\n");
    auto s = (repclient_state *) p_user_data;
    if (s->sockfd != -1) repclient_destroy(s);
    api->godot_free(s);
}

godot_variant aether_repclient_connect_to_host(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args) {
    printf("AetherRepClient._connect_to_host() %d\n", p_num_args);
    if (p_num_args != 2) { abort(); }
    auto s = (repclient_state *) p_user_data;
    assert(s->sockfd == -1);

    godot_string host_str = api->godot_variant_as_string(p_args[0]);
    godot_string port_str = api->godot_variant_as_string(p_args[1]);
    godot_char_string host_charstr = api->godot_string_ascii(&host_str);
    godot_char_string port_charstr = api->godot_string_ascii(&port_str);

    *s = repclient_init(api->godot_char_string_get_data(&host_charstr), api->godot_char_string_get_data(&port_charstr));

    api->godot_char_string_destroy(&host_charstr);
    api->godot_char_string_destroy(&port_charstr);
    api->godot_string_destroy(&host_str);
    api->godot_string_destroy(&port_str);

    // TODO: should be like StreamPeerTCP and return OK
    godot_variant ret;
    api->godot_variant_new_nil(&ret);
    return ret;
}

godot_variant aether_repclient_init_playback(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args) {
    printf("AetherRepClient._init_playback() %d\n", p_num_args);
    auto s = (repclient_state *) p_user_data;
    assert(s->sockfd == -1);

    char * here = get_current_dir_name();
    printf("here: %s\n", here);
    free(here);

    if (p_num_args == 1) {
        godot_string file_str = api->godot_variant_as_string(p_args[0]);
        godot_char_string file_charstr = api->godot_string_ascii(&file_str);

        *s = repclient_init_playback(api->godot_char_string_get_data(&file_charstr));
        api->godot_char_string_destroy(&file_charstr);
        api->godot_string_destroy(&file_str);
    } else if (p_num_args == 0)
        *s = repclient_init_playback(NULL);
    else
        abort();

    // TODO: should be like StreamPeerTCP and return OK
    godot_variant ret;
    api->godot_variant_new_nil(&ret);
    return ret;
}

godot_variant aether_repclient_send_message(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args) {
  assert(p_num_args == 1);
  auto state = (repclient_state *) p_user_data;
  assert(state->sockfd != -1);

  const godot_variant variant_data = **p_args;
  assert(api->godot_variant_get_type(&variant_data) == GODOT_VARIANT_TYPE_POOL_BYTE_ARRAY);
  godot_pool_byte_array data = api->godot_variant_as_pool_byte_array(&variant_data);
  const size_t size = api->godot_pool_byte_array_size(&data);
  godot_pool_byte_array_read_access *read = api->godot_pool_byte_array_read(&data);
  const uint8_t *data_ptr = api->godot_pool_byte_array_read_access_ptr(read);
  assert(data_ptr != NULL);

  repclient_send_message(state, (void *) data_ptr, size);
  api->godot_pool_real_array_read_access_destroy(read);
  api->godot_pool_byte_array_destroy(&data);

  godot_variant ret;
  api->godot_variant_new_nil(&ret);
  return ret;
}

godot_variant aether_repclient_try_get_msg(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args) {
    //printf("AetherRepClient._try_get_msg() %d\n", p_num_args);
    if (p_num_args != 0) abort();
    auto s = (repclient_state *) p_user_data;
    assert(s->sockfd != -1);

    uint64_t id;
    size_t msgsize;
    struct client_message *msg = (client_message *) repclient_tick(s, &id, &msgsize);
    if (msg == NULL) {
        godot_variant ret;
        api->godot_variant_new_nil(&ret);
        return ret;
    }

    godot_variant id_var;
    api->godot_variant_new_uint(&id_var, id);

    godot_pool_byte_array pba;
    api->godot_pool_byte_array_new(&pba);
    api->godot_pool_byte_array_resize(&pba, msgsize);
    godot_pool_byte_array_write_access *write_access = api->godot_pool_byte_array_write(&pba);
    uint8_t *ptr = api->godot_pool_byte_array_write_access_ptr(write_access);
    memcpy(ptr, msg, msgsize);
    api->godot_pool_byte_array_write_access_destroy(write_access);
    godot_variant pba_var;
    api->godot_variant_new_pool_byte_array(&pba_var, &pba);
    api->godot_pool_byte_array_destroy(&pba);

    godot_array retarr;
    api->godot_array_new(&retarr);
    api->godot_array_append(&retarr, &id_var);
    api->godot_array_append(&retarr, &pba_var);
    api->godot_variant_destroy(&id_var);
    api->godot_variant_destroy(&pba_var);
    godot_variant ret;
    api->godot_variant_new_array(&ret, &retarr);
    api->godot_array_destroy(&retarr);
    return ret;
}

void GDN_EXPORT godot_nativescript_init(void *p_handle) {
    printf("nativescript_init\n");
    godot_method_attributes norpc = { GODOT_METHOD_RPC_MODE_DISABLED };

    {
        godot_instance_create_func create =   { aether_repclient_constructor, NULL, NULL };
        godot_instance_destroy_func destroy = { aether_repclient_destructor, NULL, NULL };
        nativescript_api->godot_nativescript_register_class(p_handle, "AetherRepClient", "Reference", create, destroy);

        godot_instance_method try_get_msg = { aether_repclient_try_get_msg, NULL, NULL };
        nativescript_api->godot_nativescript_register_method(p_handle, "AetherRepClient", "try_get_msg", norpc, try_get_msg);

        godot_instance_method send_message = { aether_repclient_send_message, NULL, NULL };
        nativescript_api->godot_nativescript_register_method(p_handle, "AetherRepClient", "send_message", norpc, send_message);

        godot_instance_method connect_to_host = { aether_repclient_connect_to_host, NULL, NULL };
        nativescript_api->godot_nativescript_register_method(p_handle, "AetherRepClient", "connect_to_host", norpc, connect_to_host);

        godot_instance_method init_playback = { aether_repclient_init_playback, NULL, NULL };
        nativescript_api->godot_nativescript_register_method(p_handle, "AetherRepClient", "init_playback", norpc, init_playback);
    }
}
}
