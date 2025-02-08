# 指定源文件
set(SOURCE_FILES
    poller.cpp
)

# 创建一个静态库
add_library(tinynet STATIC ${SOURCE_FILES})
