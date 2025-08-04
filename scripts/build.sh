cd ../build \
    && cmake .. -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake \
    && make exchange_main \
    && make trading_main

