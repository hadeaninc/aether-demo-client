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

{ stdenv, glfw, glew, godot_headers }:
stdenv.mkDerivation {
  name               = "hadean-aether-clients";
  src                = ./.;
  buildInputs        = [ glew glfw godot_headers ];
  installPhase       = "make install DESTDIR=$out";
}
