#include "RadiantTest.h"

#include "iimage.h"
#include "RGBAImage.h"

// Helpers for examining pixel data
using RGB8 = BasicVector3<uint8_t>;

// Override operator<< to print RGB8 components as numbers, rather than random
// ASCII characters
std::ostream& operator<< (std::ostream& os, const RGB8& rgb)
{
    return os << "[" << int(rgb.x()) << ", " << int(rgb.y()) << ", "
              << int(rgb.z()) << "]";
}

namespace test
{

// Test fixture for image loading. Provides a convenient method to load an image
// relative to the test project path.
class ImageLoadingTest: public RadiantTest
{
protected:

    // Load an image from the given path
    ImagePtr loadImage(const std::string& path)
    {
        auto filePath = _context.getTestProjectPath() + path;
        return GlobalImageLoader().imageFromFile(filePath);
    }
};

TEST_F(ImageLoadingTest, LoadPng8Bit)
{
    auto img = loadImage("textures/pngs/twentyone_8bit.png");

    EXPECT_EQ(img->getWidth(), 32);
    EXPECT_EQ(img->getHeight(), 32);
}

TEST_F(ImageLoadingTest, LoadPng16Bit)
{
    auto img = loadImage("textures/pngs/twentyone_16bit.png");

    EXPECT_EQ(img->getWidth(), 32);
    EXPECT_EQ(img->getHeight(), 32);
}

TEST_F(ImageLoadingTest, LoadPngGreyscaleWithAlpha)
{
    // This is a 8-Bit Greyscale PNG with Alpha channel, so pixel depth is 16 bits
    auto img = loadImage("textures/pngs/transparent_greyscale.png");

    EXPECT_EQ(img->getWidth(), 32);
    EXPECT_EQ(img->getHeight(), 32);
    EXPECT_FALSE(img->isPrecompressed());

    // If the image loader interprets the file correctly, we should have an RGBA
    // image with the colour values being the same for R, G and B.
    // If the image loader didn't convert grey to RGB, the grey value is
    // smeared across the whole RGB channels and they are not uniform

    EXPECT_TRUE(std::dynamic_pointer_cast<RGBAImage>(img));
    auto pixels = reinterpret_cast<RGBAPixel*>(img->getPixels());

    auto numPixels = img->getWidth() * img->getHeight();
    for (auto i = 0; i < numPixels; ++i)
    {
        EXPECT_EQ(pixels[i].blue, pixels[i].green) << "Expected Green == Blue";
        EXPECT_EQ(pixels[i].red, pixels[i].green) << "Expected Red == Blue";

        if (pixels[i].blue != pixels[i].green || pixels[i].red != pixels[i].green)
        {
            break;
        }
    }
}

TEST_F(ImageLoadingTest, LoadDDSUncompressed)
{
    auto img = loadImage("textures/dds/test_16x16_uncomp.dds");

    // Check size is correct
    EXPECT_EQ(img->getWidth(), 16);
    EXPECT_EQ(img->getHeight(), 16);

    // Examine pixel data
    uint8_t* bytes = img->getPixels();
    RGB8* pixels = reinterpret_cast<RGB8*>(bytes);

    EXPECT_EQ(pixels[0], RGB8(0, 0, 0));        // border
    EXPECT_EQ(pixels[18], RGB8(255, 255, 255)); // background
    EXPECT_EQ(pixels[113], RGB8(0, 255, 0));    // green band
    EXPECT_EQ(pixels[119], RGB8(0, 0, 255));    // red centre (but BGR)
    EXPECT_EQ(pixels[255], RGB8(0, 0, 0));      // border
}

}