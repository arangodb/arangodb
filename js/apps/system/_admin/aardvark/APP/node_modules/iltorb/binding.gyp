{
  "targets": [
    {
      "target_name": "iltorb",
      "sources": [
        "brotli/common/dictionary.c",

        "brotli/dec/bit_reader.c",
        "brotli/dec/decode.c",
        "brotli/dec/huffman.c",
        "brotli/dec/state.c",

        "brotli/enc/backward_references.c",
        "brotli/enc/backward_references_hq.c",
        "brotli/enc/bit_cost.c",
        "brotli/enc/block_splitter.c",
        "brotli/enc/brotli_bit_stream.c",
        "brotli/enc/cluster.c",
        "brotli/enc/compress_fragment.c",
        "brotli/enc/compress_fragment_two_pass.c",
        "brotli/enc/dictionary_hash.c",
        "brotli/enc/encode.c",
        "brotli/enc/entropy_encode.c",
        "brotli/enc/histogram.c",
        "brotli/enc/literal_cost.c",
        "brotli/enc/memory.c",
        "brotli/enc/metablock.c",
        "brotli/enc/static_dict.c",
        "brotli/enc/utf8_util.c",

        "src/common/allocator.cc",
        "src/common/stream_coder.cc",

        "src/dec/stream_decode.cc",
        "src/dec/stream_decode_worker.cc",

        "src/enc/stream_encode.cc",
        "src/enc/stream_encode_worker.cc",

        "src/iltorb.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "brotli/include"
      ],
      "defines": ["NOMINMAX"],
      "cflags" : [
        "-O2"
      ],
      "xcode_settings": {
        "OTHER_CFLAGS" : ["-O2"]
      }
    },
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [
        "iltorb"
      ],
      "copies": [
        {
          "files": [
            "<(PRODUCT_DIR)/iltorb.node"
          ],
          "destination": "build/bindings"
        }
      ]
    }
  ]
}
