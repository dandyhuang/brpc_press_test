cc_binary(
    name = "brpc_press_test",
    srcs = glob([
        './*.cpp',
        './proto/*.cc',
    ]),
    deps = [
        '//thirdparty/leveldb-1.12.0:leveldb',
        '//thirdparty/brpc:brpc',
        '//thirdparty/gflags:gflags',
        #'//thirdparty/protobuf-3.6.1:protobuf',
        '//thirdparty/gmock:gmock',
        '//thirdparty/robin-map:robinmap',
        '//thirdparty/rdkafka:rdkafka',
        '//thirdparty/gtest:gtest',
        '//thirdparty/glog:glog',
        '//thirdparty/jsoncpp-1.8.0:jsoncpp',
        '//thirdparty/rapidjson:rapidjson',
        '//thirdparty/yaml-cpp:yaml-cpp',
        '//thirdparty/boost:boost_filesystem',
        '//thirdparty/xml:tinyxml',
        #'//thirdparty/tair:tairclientapi',
        '//thirdparty/tbsys:tbsys',
        '//thirdparty/jemalloc:jemalloc',
        #'//thirdparty/gperftools:tcmalloc_and_profiler', # for pprof
        '//thirdparty/libshmcache:shmcache',
        '//thirdparty/libfastcommon:fastcommon',
    ],
    link_all_symbols=True
)

