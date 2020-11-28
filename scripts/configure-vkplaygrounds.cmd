cmake^
 -Wdev^
 -Wdeprecated^
 -Werror=dev^
 -Werror=deprecated^
 -S"C:\devel\vkplaygrounds"^
 -B"%~dp0vkplaygrounds"^
 -G"Ninja Multi-Config"^
 -DCMAKE_DEFAULT_BUILD_TYPE:STRING=Debug^
 -DCMAKE_VS_JUST_MY_CODE_DEBUGGING:BOOL=ON^
 -DCMAKE_JOB_POOLS="pool-linking=%NUMBER_OF_PROCESSORS%;pool-compilation=%NUMBER_OF_PROCESSORS%"^
 -DCMAKE_JOB_POOL_COMPILE:STRING="pool-compilation"^
 -DCMAKE_JOB_POOL_LINK:STRING="pool-linking"^
 -DCMAKE_TOOLCHAIN_FILE:PATH="C:\devel\vcpkg\scripts\buildsystems\vcpkg.cmake"^
 %*
