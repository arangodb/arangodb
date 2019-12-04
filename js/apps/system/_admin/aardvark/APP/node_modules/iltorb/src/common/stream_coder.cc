#include "stream_coder.h"

StreamCoder::StreamCoder() {
}

StreamCoder::~StreamCoder() {
  size_t n_chunks = pending_output.size();
  for (size_t i = 0; i < n_chunks; i++) {
    alloc.Free(pending_output[i]);
  }

  alloc.ReportMemoryToV8();
}

Local<Array> StreamCoder::PendingChunksAsArray() {
  size_t n_chunks = pending_output.size();
  Local<Array> chunks = Nan::New<Array>(n_chunks);

  for (size_t i = 0; i < n_chunks; i++) {
    uint8_t* current = pending_output[i];
    Allocator::AllocatedBuffer* buf_info = Allocator::GetBufferInfo(current);
    Nan::Set(chunks, i, Nan::NewBuffer(reinterpret_cast<char*>(current),
                                       buf_info->size,
                                       Allocator::NodeFree,
                                       NULL).ToLocalChecked());
  }
  pending_output.clear();

  return chunks;
}
