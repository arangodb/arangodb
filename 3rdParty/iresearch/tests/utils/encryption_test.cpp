////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "tests_param.hpp"

#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/crc.hpp"
#include "utils/encryption.hpp"

namespace {

using irs::bstring;

void assert_encryption(size_t block_size, size_t header_lenght) {
  tests::rot13_encryption enc(block_size, header_lenght);

  bstring encrypted_header;
  encrypted_header.resize(enc.header_length());
  ASSERT_TRUE(enc.create_header("encrypted", &encrypted_header[0]));
  ASSERT_EQ(header_lenght, enc.header_length());

  bstring header = encrypted_header;
  auto cipher = enc.create_stream("encrypted", &header[0]);
  ASSERT_NE(nullptr, cipher);
  ASSERT_EQ(block_size, cipher->block_size());

  // unencrypted part of the header: counter+iv
  ASSERT_EQ(
    irs::bytes_ref(encrypted_header.c_str(), 2*cipher->block_size()),
    irs::bytes_ref(header.c_str(), 2*cipher->block_size())
  );

  // encrypted part of the header
  ASSERT_TRUE(
    encrypted_header.size() == 2*cipher->block_size()
    || (irs::bytes_ref(encrypted_header.c_str()+2*cipher->block_size(), encrypted_header.size() - 2*cipher->block_size())
        != irs::bytes_ref(header.c_str()+2*cipher->block_size(), header.size() - 2*cipher->block_size()))
  );

  const bstring data(
    reinterpret_cast<const irs::byte_type*>("4jLFtfXSuSdsGXbXqH8IpmPqx5n6IWjO9Pj8nZ0yD2ibKvZxPdRaX4lNsz8N"),
    30
  );

  // encrypt less than block size
  {
    bstring source(data.c_str(), 7);

    {
      size_t offset = 0;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 4;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 1023;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }
  }

  // encrypt size of the block
  {
    bstring source(data.c_str(), 13);

    {
      size_t offset = 0;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 4;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 1023;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }
  }

  // encrypt more than size of the block
  {
    bstring source = data;

    {
      size_t offset = 0;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 4;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }

    {
      size_t offset = 1023;
      ASSERT_TRUE(cipher->encrypt(offset, &source[0], source.size()));
      ASSERT_TRUE(cipher->decrypt(offset, &source[0], source.size()));
      ASSERT_EQ(
        irs::bytes_ref(data.c_str(), source.size()),
        irs::bytes_ref(source)
      );
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                          ctr_encryption_test_case
// -----------------------------------------------------------------------------

TEST(ctr_encryption_test, static_consts) {
  static_assert("encryption" == irs::type<irs::encryption>::name());
  static_assert(4096 == irs::ctr_encryption::DEFAULT_HEADER_LENGTH);
  static_assert(sizeof(uint64_t) == irs::ctr_encryption::MIN_HEADER_LENGTH);
}

TEST(ctr_encryption_test, create_header_stream) {
  assert_encryption(1, irs::ctr_encryption::DEFAULT_HEADER_LENGTH);
  assert_encryption(13, irs::ctr_encryption::DEFAULT_HEADER_LENGTH);
  assert_encryption(16, irs::ctr_encryption::DEFAULT_HEADER_LENGTH);
  assert_encryption(1, sizeof(uint64_t));
  assert_encryption(4, sizeof(uint64_t));
  assert_encryption(8, 2*sizeof(uint64_t));

  // block_size == 0
  {
    tests::rot13_encryption enc(0);

    bstring encrypted_header;
    ASSERT_FALSE(enc.create_header("encrypted", &encrypted_header[0]));
    ASSERT_FALSE(enc.create_stream("encrypted", &encrypted_header[0]));
  }

  // header is too small (< MIN_HEADER_LENGTH)
  {
    tests::rot13_encryption enc(1, 7);

    bstring encrypted_header;
    encrypted_header.resize(enc.header_length());
    ASSERT_FALSE(enc.create_header("encrypted", &encrypted_header[0]));
    ASSERT_FALSE(enc.create_stream("encrypted", &encrypted_header[0]));
  }

  // header is too small (< 2*block_size)
  {
    tests::rot13_encryption enc(13, 25);

    bstring encrypted_header;
    encrypted_header.resize(enc.header_length());
    ASSERT_FALSE(enc.create_header("encrypted", &encrypted_header[0]));
    ASSERT_FALSE(enc.create_stream("encrypted", &encrypted_header[0]));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                              encryption_test_case
// -----------------------------------------------------------------------------

class encryption_test_case : public tests::directory_test_case_base {
 protected:
  void assert_ecnrypted_streams(size_t block_size, size_t header_length, size_t buf_size) {
    std::vector<std::string> const data{
      "spM42fEO88t2","jNIvCMksYwpoxN","Re5eZWCkQexrZn","jjj003oxVAIycv","N9IJuRjFSlO8Pa","OPGG6Ic3JYJyVY","ZDGVji8xtjh9zI","DvBDXbjKgIfPIk",
      "bZyCbyByXnGvlL","vjGDbNcZGDmQ2","J7by8eYg0ZGbYw","6UZ856mrVW9DeD","Ny6bZIbGQ43LSU","oaYAsO0tXnNBkR","Fr97cjyQoTs9Pf","7rLKaQN4REFIgn",
      "EcFMetqynwG87T","oshFa26WK3gGSl","8keZ9MLvnkec8Q","HuiOGpLtqn79GP","Qnlj0JiQjBR3YW","k64uvviemlfM8p","32X34QY6JaCH3L","NcAU3Aqnn87LJW",
      "Q4LLFIBU9ci40O","M5xpjDYIfos22t","Te9ZhWmGt2cTXD","HYO3hJ1C4n1DvD","qVRj2SyXcKQz3Z","vwt41rzEW7nkoi","cLqv5U8b8kzT2H","tNyCoJEOm0POyC",
      "mLw6cl4HxmOHa","2eTVXvllcGmZ0e","NFF9SneLv6pX8h","kzCvqOVYlYA3QT","mxCkaGg0GeLxYq","PffuwSr8h3acP0","zDm0rAHgzhHsmv","8LYMjImx00le9c",
      "Ju0FM0mJmqkue1","uNwn8A2SH4OSZW","R1Dm21RTJzb0aS","sUpQGy1n6TiH82","fhkCGcuQ5VnrEa","b6Xsq05brtAr88","NXVkmxvLmhzFRY","s9OuZyZX28uux0",
      "DQaD4HyDMGkbg3","Fr2L3V4UzCZZcJ","7MgRPt0rLo6Cp4","c8lK5hjmKUuc3e","jzmu3ZcP3PF62X","pmUUUvAS00bPfa","lonoswip3le6Hs","TQ1G0ruVvknb8A",
      "4XqPhpJv","gY0QFCjckHp1JI","v2a0yfs9yN5lY4","l1XKKtBXtktOs2","AGotoLgRxPe4Pr","x9zPgBi3Bw8DFD","OhX85k7OhY3FZM","riRP6PRhkq0CUi",
      "1ToW1HIephPBlz","C8xo1SMWPZW8iE","tBa3qiFG7c1wiD","BRXFbUYzw646PS","mbR0ECXCash1rF","AVDjHnwujjOGAK","16bmhl4gvDpj44","OLa0D9RlpBLRgK",
      "PgCSXvlxyHQFlQ","sMyrmGRcVTwg53","Fa6Fo687nt9bDV","P0lUFttS64mC7s","rxTZUQIpOPYkPp","oNEsVpak9SNgLh","iHmFTSjGutROen","aTMmlghno9p91a",
      "tpb3rHs9ZWtL5m","iG0xrYN7gXXPTs","KsEl2f8WtF6Ylv","triXFZM9baNltC","MBFTh22Yos3vGt","DTuFyue5f9Mk3x","v2zm4kYxfar0J7","xtpwVgOMT0eIFS",
      "8Wz7MrtXkSH9CA","FuURHWmPLb","YpIFnExqjgpSh0","2oaIkTM6EJ","s16qvfbrycGnVP","yUb2fcGIDRSujG","9rIfsuCyTCTiLY","HXTg5jWrVZNLNP",
      "maLjUi6Oo6wsJr","C6iHChfoJHGxzO","6LxzytT8iSzNHZ","ex8znLIzbatFCo","HiYTSzZhBHgtaP","H5EpiJw2L5UgD1","ZhPvYoUMMFkoiL","y6014BfgqbE3ke",
      "XXutx8GrPYt7Rq","DjYwLMixhS80an","aQxh91iigWOt4x","1J9ZC2r7CCfGIH","Sg9PzDCOb5Ezym","4PB3SukHVhA6SB","BfVm1XGLDOhabZ","ChEvexTp1CrLUL",
      "M5nlO4VcxIOrxH","YO9rnNNFwzRphV","KzQhfZSnQQGhK9","r7Ez7Zwr0bn","fQipSie8ZKyT62","3yyLqJMcShXG9z","UTb12lz3k5xPPt","JjcWQnBnRFJ2Mv",
      "zsKEX7BLJQTjCx","g0oPvTcOhiev1k","8P6HF4I6t1jwzu","LaOiJIU47kagqu","pyY9sV9WQ5YuQC","MCgpgJhEwrGKWM","Hq5Wgc3Am8cjWw","FnITVHg0jw03Bm",
      "0Jq2YEnFf52861","y0FT03yG9Uvg6I","S6uehKP8uj6wUe","usC8CZobBmuk6","LrZuchHNpSs282","PsmFFySt7fKFOv","mXe9j6xNYttnSy","al9J6AZYlhAlWU",
      "3v8PsohUeKegJI","QZCwr1URS1OWzX","UVCg1mVWmSBWRT","pO2lnQ4L6yHQic","w5EtZl2gZhj2ca","04B62aNIpnBslQ","0Sz6UCGXBwi7At","l49gEiyDkc3J00",
      "2T9nyWrRwuZj9W","CTtHTPRhSAPRIW","sJZI3K8vP96JPm","HYEy1brtrtrBJskEYa2","UKb3uiFuGEi7m9","yeRCnG0EEZ8Vrr"
    };
    const size_t magic = 0x43219876;

    ASSERT_FALSE(dir().attributes().contains<irs::encryption>());
    auto& enc = dir().attributes().emplace<tests::rot13_encryption>(block_size, header_length);

    uint64_t fp_magic = 0;
    uint64_t encrypted_length = 0;
    uint64_t checksum = 0;

    // write encrypted data
    {
      bstring header(enc->header_length(), 0);
      ASSERT_TRUE(enc->create_header("encrypted", &header[0]));

      auto out = dir().create("encrypted");
      auto raw_out = dir().create("raw");
      ASSERT_NE(nullptr, out);
      irs::write_string(*out, header);
      ASSERT_EQ(irs::bytes_io<uint64_t>::vsize(header.size()) + header.size(), out->file_pointer());
      auto cipher = enc->create_stream("encrypted", &header[0]);
      ASSERT_NE(nullptr, cipher);
      ASSERT_EQ(block_size, cipher->block_size());

      irs::encrypted_output encryptor(std::move(out), *cipher, buf_size);
      ASSERT_EQ(nullptr, out);
      ASSERT_EQ(enc->header_length() + irs::bytes_io<uint64_t>::vsize(header.size()), encryptor.stream().file_pointer());
      ASSERT_EQ(std::max(buf_size,size_t(1))*cipher->block_size(), encryptor.buffer_size());
      ASSERT_EQ(0, encryptor.file_pointer());

      size_t step = 321;
      for (auto& str : data) {
        irs::write_string(encryptor, str);
        irs::write_string(*raw_out, str);
      }

      fp_magic = encryptor.file_pointer();

      encryptor.write_long(magic);
      raw_out->write_long(magic);

      for (size_t i = 0, step = 321; i < data.size(); ++i) {
        auto value = 9886 + step;
        encryptor.write_vlong(value);
        raw_out->write_vlong(value);

        step += step;
        value = 9886 + step;

        encryptor.write_long(value);
        raw_out->write_long(value);

        step += step;
      }

      encryptor.flush();
      ASSERT_EQ(raw_out->file_pointer(), encryptor.file_pointer());
      encrypted_length = encryptor.file_pointer();
      checksum = raw_out->checksum();
      out = encryptor.release();
      ASSERT_NE(nullptr, out);
      ASSERT_EQ(out->file_pointer(), irs::bytes_io<uint64_t>::vsize(header.size()) + header.size() + encrypted_length);
    }

    // read encrypted data
    {
      auto in = dir_->open("encrypted", irs::IOAdvice::NORMAL);
      bstring header = irs::read_string<bstring>(*in);
      ASSERT_EQ(irs::bytes_io<uint64_t>::vsize(header.size()) + header.size() + encrypted_length, in->length());
      ASSERT_EQ(enc->header_length(), header.size());
      ASSERT_EQ(enc->header_length() + irs::bytes_io<uint64_t>::vsize(header.size()), in->file_pointer());

      auto cipher = enc->create_stream("encrypted", &header[0]);
      ASSERT_NE(nullptr, cipher);
      ASSERT_EQ(block_size, cipher->block_size());

      irs::encrypted_input decryptor(std::move(in), *cipher, buf_size);
      ASSERT_EQ(nullptr, in);
      ASSERT_EQ(enc->header_length() + irs::bytes_io<uint64_t>::vsize(header.size()), decryptor.stream().file_pointer());
      ASSERT_EQ(std::max(buf_size,size_t(1))*cipher->block_size(), decryptor.buffer_size());
      ASSERT_EQ(0, decryptor.file_pointer());

      decryptor.seek(fp_magic);
      ASSERT_EQ(magic, decryptor.read_long());
      ASSERT_EQ(fp_magic + sizeof(uint64_t), decryptor.file_pointer());
      decryptor.seek(0);

      // check dup
      {
        auto dup = decryptor.dup();
        dup->seek(fp_magic);
        ASSERT_EQ(magic, dup->read_long());
        ASSERT_EQ(fp_magic + sizeof(uint64_t), dup->file_pointer());
        for (size_t i = 0, step = 321; i < data.size(); ++i) {
          ASSERT_EQ(9886 + step, dup->read_vlong());
          step += step;
          ASSERT_EQ(9886 + step, dup->read_long());
          step += step;
        }
      }

      // checksum
      {
        auto dup = decryptor.reopen();
        dup->seek(0);
        ASSERT_EQ(checksum, dup->checksum(dup->length()));
        ASSERT_EQ(0, dup->file_pointer()); // checksum doesn't change position
        ASSERT_EQ(checksum, dup->checksum(std::numeric_limits<size_t>::max()));
        ASSERT_EQ(0, dup->file_pointer()); // checksum doesn't change position
      }

      // check reopen
      {
        auto dup = decryptor.reopen();
        dup->seek(fp_magic);
        ASSERT_EQ(magic, dup->read_long());
        ASSERT_EQ(fp_magic + sizeof(uint64_t), dup->file_pointer());
        for (size_t i = 0, step = 321; i < data.size(); ++i) {
          ASSERT_EQ(9886 + step, dup->read_vlong());
          step += step;
          ASSERT_EQ(9886 + step, dup->read_long());
          step += step;
        }
      }

      // checksum
      {
        auto dup = decryptor.reopen();
        dup->seek(0);
        ASSERT_EQ(checksum, dup->checksum(dup->length()));
        ASSERT_EQ(0, dup->file_pointer()); // checksum doesn't change position
        ASSERT_EQ(checksum, dup->checksum(std::numeric_limits<size_t>::max()));
        ASSERT_EQ(0, dup->file_pointer()); // checksum doesn't change position
      }

      for (auto& str : data) {
        const auto fp = decryptor.file_pointer();
        ASSERT_EQ(str, irs::read_string<std::string>(decryptor));
        decryptor.seek(fp);
        ASSERT_EQ(str, irs::read_string<std::string>(decryptor));
      }
      ASSERT_EQ(fp_magic, decryptor.file_pointer());
      ASSERT_EQ(magic, decryptor.read_long());

      for (size_t i = 0, step = 321; i < data.size(); ++i) {
        ASSERT_EQ(9886 + step, decryptor.read_vlong());
        step += step;
        ASSERT_EQ(9886 + step, decryptor.read_long());
        step += step;
      }

      const auto fp = decryptor.file_pointer();
      in = decryptor.release();
      ASSERT_NE(nullptr, in);
      ASSERT_EQ(in->file_pointer(), header_length + irs::bytes_io<uint64_t>::vsize(header.size()) + fp);
    }

    ASSERT_TRUE(dir().attributes().remove<irs::encryption>());
  }
};

TEST_P(encryption_test_case, encrypted_io) {
  assert_ecnrypted_streams(13, irs::ctr_encryption::DEFAULT_HEADER_LENGTH, 0);
  assert_ecnrypted_streams(13, irs::ctr_encryption::DEFAULT_HEADER_LENGTH, 1);
  assert_ecnrypted_streams(7, irs::ctr_encryption::DEFAULT_HEADER_LENGTH, 5);
  assert_ecnrypted_streams(16, irs::ctr_encryption::DEFAULT_HEADER_LENGTH, 64);
  assert_ecnrypted_streams(2048, irs::ctr_encryption::DEFAULT_HEADER_LENGTH, 1);
}

TEST(ecnryption_test_case, ensure_no_double_bufferring) {
  class buffered_output : public irs::buffered_index_output {
   public:
    buffered_output(index_output& out) noexcept
      : out_(&out) {
      buffered_index_output::reset(buf_, sizeof buf_);
    }

    virtual int64_t checksum() const override {
      return out_->checksum();
    }

    const index_output& stream() const {
      return *out_;
    }

    using irs::buffered_index_output::remain;

    size_t last_written_size() const noexcept {
      return last_written_size_;
    }

   protected:
    virtual void flush_buffer(const irs::byte_type* b, size_t size) override {
      last_written_size_ = size;
      out_->write_bytes(b, size);
    }

    irs::byte_type buf_[irs::DEFAULT_ENCRYPTION_BUFFER_SIZE];
    index_output* out_;
    size_t last_written_size_{};
  };

  class buffered_input final : public irs::buffered_index_input {
   public:
    buffered_input(index_input& in) noexcept
      : in_(&in) {
      irs::buffered_index_input::reset(buf_, sizeof buf_, 0);
    }

    const index_input& stream() {
      return *in_;
    }

    virtual size_t length() const override {
      return in_->length();
    }

    virtual index_input::ptr dup() const override {
      throw irs::not_impl_error();
    }

    virtual index_input::ptr reopen() const override {
      throw irs::not_impl_error();
    }

    virtual int64_t checksum(size_t offset) const override {
      return in_->checksum(offset);
    }

    using irs::buffered_index_input::remain;

    size_t last_read_size() const noexcept {
      return last_read_size_;
    }

   protected:
    virtual void seek_internal(size_t pos) override {
      in_->seek(pos);
    }

    virtual size_t read_internal(irs::byte_type* b, size_t size) override {
      last_read_size_ = size;
      return in_->read_bytes(b, size);
    }

   private:
    irs::byte_type buf_[irs::DEFAULT_ENCRYPTION_BUFFER_SIZE];
    index_input* in_;
    size_t last_read_size_{};
  };

  tests::rot13_encryption enc(16);
  irs::memory_output out(irs::memory_allocator::global());

  bstring encrypted_header;
  encrypted_header.resize(enc.header_length());
  ASSERT_TRUE(enc.create_header("encrypted", &encrypted_header[0]));
  ASSERT_EQ(size_t(tests::rot13_encryption::DEFAULT_HEADER_LENGTH), enc.header_length());

  bstring header = encrypted_header;
  auto cipher = enc.create_stream("encrypted", &header[0]);
  ASSERT_NE(nullptr, cipher);
  ASSERT_EQ(16, cipher->block_size());

  {
    buffered_output buf_out(out.stream);
    irs::encrypted_output enc_out(buf_out, *cipher, irs::DEFAULT_ENCRYPTION_BUFFER_SIZE/cipher->block_size());
    ASSERT_EQ(nullptr, enc_out.release()); // unmanaged instance

    for (size_t i = 0; i < 2*irs::DEFAULT_ENCRYPTION_BUFFER_SIZE+1; ++i) {
      enc_out.write_vint(i);
      ASSERT_EQ(size_t(irs::DEFAULT_ENCRYPTION_BUFFER_SIZE), buf_out.remain()); // ensure no buffering
      if (buf_out.file_pointer() >= irs::DEFAULT_ENCRYPTION_BUFFER_SIZE) {
        ASSERT_EQ(size_t(irs::DEFAULT_ENCRYPTION_BUFFER_SIZE), buf_out.last_written_size());
      }
    }

    enc_out.flush();
    buf_out.flush();
    ASSERT_EQ(enc_out.file_pointer() - 3*irs::DEFAULT_ENCRYPTION_BUFFER_SIZE, buf_out.last_written_size());
  }

  out.stream.flush();

  {
    irs::memory_index_input in(out.file);
    buffered_input buf_in(in);
    irs::encrypted_input enc_in(buf_in, *cipher, irs::DEFAULT_ENCRYPTION_BUFFER_SIZE/cipher->block_size());

    for (size_t i = 0; i < 2*irs::DEFAULT_ENCRYPTION_BUFFER_SIZE+1; ++i) {
      ASSERT_EQ(i, enc_in.read_vint());
      ASSERT_EQ(0, buf_in.remain()); // ensure no buffering
      if (buf_in.file_pointer() <= 3*irs::DEFAULT_ENCRYPTION_BUFFER_SIZE) {
        ASSERT_EQ(size_t(irs::DEFAULT_ENCRYPTION_BUFFER_SIZE), buf_in.last_read_size());
      } else {
        ASSERT_EQ(buf_in.length() - 3*irs::DEFAULT_ENCRYPTION_BUFFER_SIZE, buf_in.last_read_size());
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(
  encryption_test,
  encryption_test_case,
  ::testing::Values(
    &tests::memory_directory,
    &tests::fs_directory,
    &tests::mmap_directory
  ),
  tests::directory_test_case_base::to_string
);

}
