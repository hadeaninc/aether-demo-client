# Copyright 2018 Hadean Supercomputing Inc.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

extends Node2D

var USE_C_REPCLIENT = null

var repclient = null

func _ready():
    set_process(true)

var mytexture = preload("res://ball.png")

func _process(delta):
    var cells = $engine.recv_messages()
    for child in get_children():
        if child.name == "engine": continue
        child.free()
    if not cells.has(0):
        return

    var grid_scale = 20

    for cell_info_and_points in cells.values():
        var points = cell_info_and_points[1]
        for point in points:
            var point_xy = [point[0][0], point[0][1]] # clone array
            point_xy[0] += 20
            point_xy[1] += 8
            point_xy[0] *= grid_scale
            point_xy[1] *= grid_scale

            var point_color = point[1]
            var s = Sprite.new()
            s.texture = mytexture
            s.position = Vector2(point_xy[0], point_xy[1])
            s.modulate = Color(point_color[0], point_color[1], point_color[2], 1.0)
            add_child(s)
