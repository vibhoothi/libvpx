#include <memory>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "vp9/simple_encode.h"

namespace {

TEST(SimpleEncode, ComputeFirstPassStats) {
  int w = 352;
  int h = 288;
  int frame_rate_num = 30;
  int frame_rate_den = 1;
  int target_bitrate = 200;
  int num_frames = 17;
  // TODO(angiebird): Figure out how to upload test video to our codebase
  FILE *file = fopen("bus_352x288_420_f20_b8.yuv", "r");
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, file);
  simple_encode.ComputeFirstPassStats();
  std::vector<std::vector<double>> frame_stats =
      simple_encode.ObserveFirstPassStats();
  EXPECT_EQ(frame_stats.size(), static_cast<size_t>(num_frames));
  size_t data_num = frame_stats[0].size();
  // Read ObserveFirstPassStats before changing FIRSTPASS_STATS.
  EXPECT_EQ(data_num, static_cast<size_t>(25));
  for (size_t i = 0; i < frame_stats.size(); ++i) {
    EXPECT_EQ(frame_stats[i].size(), data_num);
    // FIRSTPASS_STATS's first element is frame
    EXPECT_EQ(frame_stats[i][0], i);
    // FIRSTPASS_STATS's last element is count, and the count is 1 for single
    // frame stats
    EXPECT_EQ(frame_stats[i][data_num - 1], 1);
  }
}

TEST(SimpleEncode, GetCodingFrameNum) {
  int w = 352;
  int h = 288;
  int frame_rate_num = 30;
  int frame_rate_den = 1;
  int target_bitrate = 200;
  int num_frames = 17;
  // TODO(angiebird): Figure out how to upload test video to our codebase
  FILE *file = fopen("bus_352x288_420_f20_b8.yuv", "r");
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, file);
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_EQ(num_coding_frames, 19);
}

TEST(SimpleEncode, EncodeFrame) {
  int w = 352;
  int h = 288;
  int frame_rate_num = 30;
  int frame_rate_den = 1;
  int target_bitrate = 1000;
  int num_frames = 17;
  // TODO(angiebird): Figure out how to upload test video to our codebase
  FILE *file = fopen("bus_352x288_420_f20_b8.yuv", "r");
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, file);
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_GE(num_coding_frames, num_frames);
  // The coding frames include actual show frames and alternate reference
  // frames, i.e. no show frame.
  int ref_num_alternate_refereces = num_coding_frames - num_frames;
  int num_alternate_refereces = 0;
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameResult encode_frame_result;
    if (i == 0) {
      EXPECT_EQ(encode_frame_result.show_idx, 0);
      EXPECT_EQ(encode_frame_result.frame_type, kKeyFrame)
          << "The first coding frame should be key frame";
    }
    simple_encode.EncodeFrame(&encode_frame_result);
    if (encode_frame_result.frame_type == kAlternateReference) {
      ++num_alternate_refereces;
    }
    EXPECT_GE(encode_frame_result.show_idx, 0);
    EXPECT_LT(encode_frame_result.show_idx, num_frames);
    if (i == num_coding_frames - 1) {
      EXPECT_EQ(encode_frame_result.show_idx, num_frames - 1)
          << "The last coding frame should be the last display order";
    }
    EXPECT_GE(encode_frame_result.psnr, 34)
        << "The psnr is supposed to be greater than 34 given the "
           "target_bitrate 1000 kbps";
  }
  EXPECT_EQ(num_alternate_refereces, ref_num_alternate_refereces);
  simple_encode.EndEncode();
}

}  // namespace
