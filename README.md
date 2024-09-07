<!-- SPDX-License-Identifier: zlib-acknowledgement -->
# App 
![App Lint and Test](https://github.com/ryan-mcclue/app/actions/workflows/app.yml/badge.svg)

Music visualiser app

## Linux
```
# Build raylib dependency
sudo apt install -y build-essential git cmake libasound2-dev libx11-dev libxrandr-dev \
                    libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev \
                    libxinerama-dev libwayland-dev libxkbcommon-dev
mkdir -p "build"
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -B "build" -S "code/external/raylib-5.0"
cmake --build "build"
sudo cmake --install "build" 

# Optional if want release build
mkdir private
echo 'param_mode="release"' > private/build-params

# Build and run tests
bash misc/build "tests"
./build/app-tests-debug

# Build and run app
bash misc/build "app"
./build/app-debug
```
