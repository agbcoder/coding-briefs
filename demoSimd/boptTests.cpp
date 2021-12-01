#include "bopt_avx2.h"
#include "bopt_avx2_priv.h"

#include "gtest/gtest.h"

namespace demoSimd {

// The fixture for testing class Foo.
class boptTest : public ::testing::Test {
    protected:
    // You can remove any or all of the following functions if their bodies would
    // be empty.

    boptTest() {
        // You can do set-up work for each test here.
    }

    ~boptTest() override {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override {
        // Code here will be called immediately after the constructor (right before each test).

        num_pixels_model = 32*2;
        CreateChannelArrays( num_pixels_model );

        // Currently we use 2 different model arrays: fixed and variable alpha
        modelAlphaFixed = CreateModelArray(true, 0xFF);
        modelAlphaArray = CreateModelArray(false);
    }

    void TearDown() override {
        // Code here will be called immediately after each test (right before the destructor).

        if ( modelAlphaFixed ) { free(modelAlphaFixed); modelAlphaFixed = NULL; }
        if ( modelAlphaArray ) { free(modelAlphaArray); modelAlphaArray = NULL; }

        free(r); free(g); free(b); free(a);
        r = g = b = a = NULL;

        return;
    }

    // Class members declared here can be used by all tests in the test suite.

    // Create the arrays containing the source data: the per-channel data
    void CreateChannelArrays(int num_pixels) {
        r = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
        g = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
        b = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
        a = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );

        // slow but not error-prone...
        for(unsigned int i = 0; i<num_pixels; i++) {
            r[i] = 0x40 + i;
            g[i] = 0x80 + i;
            b[i] = 0xC0 + i;
            a[i] = 0x00 + i;
        }

    }

    // Creates the expected result array using an slow, but error-free, code
    unsigned char *CreateModelArray(bool alphaFixed = false, unsigned char alphaValue = 0xFF) {
        unsigned char *model = (unsigned char *) aligned_alloc( align_forced, num_pixels_model * 4*sizeof(unsigned char) );

        unsigned char *pmodel = model;
        unsigned char *pr = r, *pg = g, *pb = b;
        unsigned char *pa = a;          // this one not used if alpha is fixed
        for(unsigned int i = 0; i<num_pixels_model; i++) {
            *pmodel++ = (alphaFixed ? alphaValue : *pa++);
            *pmodel++ = *pr++;
            *pmodel++ = *pg++;
            *pmodel++ = *pb++;
        }

        return model;
    }

    bool CompareResultArrays(
            const unsigned char *result,    // array to be tested
            const unsigned char *model,     // model array, safely built
            const int num_pixels,           // size of each channel (size of the array is x4)
            const int model_offset = 0,     // optional offset in the model array to start with
            const int bytes_per_pixel = 4   // size of one interleaved pixel
            )
    {
        model += model_offset * bytes_per_pixel;
        for(unsigned int i=0; i<num_pixels; i++) {
            if ( *result++ != *model++ ) return false;
        }

        return true;
    }

    const int align_forced = 32;

    int num_pixels_model;             // num of pixels defined
    unsigned char *a, *r, *g, *b;     // each of the 4 single channels: RGB + alpha
    unsigned char *modelAlphaFixed;   // the interleaved data in format ARGB; stores the expected result.
    unsigned char *modelAlphaArray;   // the interleaved data in format ARGB; stores the expected result.
};

/* The 'Spez' ones test the spezialized function. The 'Gen' ones test the generic, higher level ones. */

TEST_F(boptTest, Dst32_Src32_Num32Y_AlphaNotFixed_Spez_intrinsics) {
    bool res;

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced, num_pixels_model * 4*sizeof(unsigned char) );

    channels_ileaved_d32_s32_n32m_8b_intrinsics(result, a, r, g, b, num_pixels_model);
    res = CompareResultArrays(result, modelAlphaArray, num_pixels_model);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, Dst32_Src32_Num32Y_AlphaFixed_Spez_intrinsics) {
    bool res;

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced, num_pixels_model * 4*sizeof(unsigned char) );

    channels_ileaved_d32_s32_n32m_8b_intrinsics(result, NULL, r, g, b, num_pixels_model);
    res = CompareResultArrays(result, modelAlphaFixed, num_pixels_model);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num32Y_AlphaNotFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2;
    ASSERT_TRUE( (num_pixels >= 32) && ((num_pixels % 32) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n32m_8b_intrinsics(result+misalignment,
            a+misalignment, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaArray, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num32Y_AlphaFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2;
    ASSERT_TRUE( (num_pixels >= 32) && ((num_pixels % 32) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n32m_8b_intrinsics(result+misalignment,
            NULL, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaFixed, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num16Y_AlphaNotFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2 / 2;      // 16-byte aligned
    ASSERT_TRUE( (num_pixels >= 16) && ((num_pixels % 16) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n16m_8b_intrinsics(result+misalignment,
            a+misalignment, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaArray, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num16Y_AlphaFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2 / 2;      // 16-byte aligned
    ASSERT_TRUE( (num_pixels >= 16) && ((num_pixels % 16) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n16m_8b_intrinsics(result+misalignment,
            NULL, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaFixed, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num08Y_AlphaNotFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2 / 4;      // 8-byte aligned
    ASSERT_TRUE( (num_pixels >= 8) && ((num_pixels % 8) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n08m_8b_intrinsics(result+misalignment,
            a+misalignment, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaArray, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, DstMis_SrcMis_Num08Y_AlphaFixed_Spez_intrinsics) {
    bool res;

    int misalignment = 1;
    int num_pixels = num_pixels_model / 2 / 4;      // 8-byte aligned
    ASSERT_TRUE( (num_pixels >= 8) && ((num_pixels % 8) == 0) );

    unsigned char *result = (unsigned char *) aligned_alloc( align_forced,
                                    (num_pixels + misalignment) * 4*sizeof(unsigned char) );

    channels_ileaved_dmis_smis_n08m_8b_intrinsics(result+misalignment,
            NULL, r+misalignment, g+misalignment, b+misalignment, num_pixels);
    res = CompareResultArrays(result+misalignment, modelAlphaFixed, num_pixels, misalignment);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, Dst32_Src32_Num32Y_AlphaNotFixed_Generic) {
    bool res;

    CreateModelArray(false);
    unsigned char *result = (unsigned char *) aligned_alloc( align_forced, num_pixels_model * 4*sizeof(unsigned char) );

    channels_to_interleaved_8b(result, a, r, g, b, num_pixels_model);
    res = CompareResultArrays(result, modelAlphaArray, num_pixels_model);
    EXPECT_TRUE( res );

    free( result );
}

TEST_F(boptTest, Dst32_Src32_Num32Y_AlphaFixed_Generic) {
    bool res;

    CreateModelArray(false);
    unsigned char *result = (unsigned char *) aligned_alloc( align_forced, num_pixels_model * 4*sizeof(unsigned char) );

    channels_to_interleaved_8b(result, NULL, r, g, b, num_pixels_model);
    res = CompareResultArrays(result, modelAlphaFixed, num_pixels_model);
    EXPECT_TRUE( res );

    free( result );
}


}  // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
