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

extends Node

const CLIENT_HEADER_SIZE = 48 # cell, num_points, status
const POINT_SIZE = 12

var repclient
# id -> [((cell_position, cell_size), [position, color, size])]
var cells = {}

func _ready():
	var addr_var = OS.get_environment("GODOT_ENGINE_ADDR")
	var file_var = OS.get_environment("GODOT_REPLAY_FILE")

	repclient = load("res://lib/repclient/aether_repclient.gdns").new()

	if addr_var != "":
		var host_and_port = addr_var.split(":")
		var host = host_and_port[0]
		var port = host_and_port[1].to_int()
		print("Using simulation engine address: ", host, ":", port)
		repclient.connect_to_host(host, port)
	elif file_var != "":
		repclient.init_playback(file_var)
	else:
		print("====================================")
		print("WARNING: NOT CONNECTING TO REPCLIENT")
		print("====================================")
		repclient = null

	print("Connected to engine")

func recv_messages():
	if repclient == null: return cells
	while true:
		var id_and_msg = repclient.try_get_msg()
		if id_and_msg == null: break
		cells[id_and_msg[0]] = process_message(id_and_msg[1])
	return cells

func process_message(message):
	var cell_code = bytestou64(message.subarray(0, 7))
	var cell_level = bytestou64(message.subarray(8, 15))
	var numpoints = bytestou64(message.subarray(16, 23))
	var status = bytestou64(message.subarray(24, 31))

	var cell_position = morton_2_decode(cell_code)
	cell_position = Vector2(cell_position.x, cell_position.y)
	var cell_size = (1 << cell_level)
	var cell_info = [cell_position, cell_size, status]

	var points = []
	for i in range(numpoints):
		var position = net_decode_position(bytestou32(message.subarray(CLIENT_HEADER_SIZE + i*POINT_SIZE, CLIENT_HEADER_SIZE + i*POINT_SIZE + 4 - 1)), cell_code, cell_level)
		var color    = net_decode_color(bytestou32(message.subarray(CLIENT_HEADER_SIZE + i*POINT_SIZE + 4, CLIENT_HEADER_SIZE + i*POINT_SIZE + 4 + 4 - 1)))
		points.append([position, color, 6])
	return [cell_info, points]

func net_decode_position(p, cell_code, cell_level):
	var cell_xyz = morton_2_decode(cell_code)
	var cell_dim = 1 << cell_level
	var v = Vector3(
		(p >>  0) & ((1 << 10) - 1),
		(p >> 10) & ((1 << 10) - 1),
		(p >> 20) & ((1 << 10) - 1))
	return (v / (1 << 10)) * cell_dim + cell_xyz

func net_encode_position(v, cell_code, cell_level):
	var size = 1 << cell_level
	var c = morton_2_decode(cell_code)
	v = ((v - c) / size) * (1 << 10)
	return (v.x << 0) | (v.y << 10) | (v.z << 20)

func net_decode_color(c):
	return Color((c << 8) | 255)

func morton_2_decode(m):
	return Vector3(
		float(u32toi32(compact_bits_2(m))),
		float(u32toi32(compact_bits_2(m >> 1))),
		0)

func submit_event_buffer(buf):
	if repclient == null: return
	var event_size = 14
	var remaining = event_size - buf.get_size()
	for i in range(remaining): buf.put_u8(0)
	buf.seek(0)
	var data_result = buf.get_data(event_size)
	assert data_result[0] == OK
	repclient.send_message(data_result[1])

func report_bark(pos):
	var buf = StreamPeerBuffer.new()
	buf.put_u32(1)       # aether_event_type_t
	buf.put_u8(0)        # aether_mouse_click_t.button
	buf.put_u8(0)        # aether_mouse_click_t.action
	buf.put_float(pos.x) # aether_mouse_click_t.position.x
	buf.put_float(pos.y) # aether_mouse_click_t.position.y
	submit_event_buffer(buf)

func report_move(pos):
	var buf = StreamPeerBuffer.new()
	buf.put_u32(0)       # aether_event_type_t
	buf.put_float(pos.x) # aether_cursor_move_t.x
	buf.put_float(pos.y) # aether_cursor_move_t.y
	submit_event_buffer(buf)

func compact_bits_2(x):
	x &= 0x5555555555555555
	x = (x ^ (x >>  1)) & 0x3333333333333333
	x = (x ^ (x >>  2)) & 0x0f0f0f0f0f0f0f0f
	x = (x ^ (x >>  4)) & 0x00ff00ff00ff00ff
	x = (x ^ (x >>  8)) & 0x0000ffff0000ffff
	x = (x ^ (x >> 16)) & 0x00000000ffffffff
	return x

var tyconvbuf = StreamPeerBuffer.new()
func u32toi32(x):
	tyconvbuf.seek(0)
	tyconvbuf.put_u32(x)
	tyconvbuf.seek(0)
	return tyconvbuf.get_32()

func bytestou32(bs):
	assert bs.size() == 4
	return bs[0] << 0 | bs[1] << 8 | bs[2] << 16 | bs[3] << 24
func bytestou64(bs):
	assert bs.size() == 8
	return bs[0] << 0 | bs[1] << 8 | bs[2] << 16 | bs[3] << 24 | bs[4] << 32 | bs[5] << 40 | bs[6] << 48 | bs[7] << 56
