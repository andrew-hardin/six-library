/* =========================================================================
 * This file is part of six.sidd-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2018, MDA Information Systems LLC
 *
 * six.sidd-c++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */

// Test program for SIDDByteProvider
// Demonstrates that the raw bytes provided by this class result in equivalent
// SIDDs to the normal writes via NITFWriteControl

#include <stdlib.h>

#include <iostream>
#include <limits>
#include <vector>
#include <memory>
#include <std/cstddef>
#include <std/filesystem>
#include <std/optional>

#include <types/RowCol.h>
#include <gsl/gsl.h>
#include <io/FileOutputStream.h>
#include <io/ReadUtils.h>
#include <math/Round.h>

#include <nitf/J2KCompressionParameters.hpp>
#include <nitf/J2KCompressor.hpp>
#include <nitf/CompressedByteProvider.hpp>
#include <nitf/ImageWriter.hpp>
#include <nitf/WriteHandler.hpp>
#include <nitf/IOHandle.hpp>
#include <nitf/IOStreamWriter.hpp>
#include <nitf/Reader.hpp>
#include <nitf/Utils.hpp>
#include <nitf/Writer.hpp>
#include <nitf/UnitTests.hpp>

#include "TestCase.h"

static std::string testName;

static void setCornersFromDMSBox(nitf::ImageSubheader& header)
{
    /*
     *  You could do this in degrees as easily
     *  but this way we get to show off some new utilities
     */
    constexpr int latTopDMS[] = { 42, 17, 50 };
    const auto latTopDecimal = nitf::Utils::geographicToDecimal(latTopDMS[0], latTopDMS[1], latTopDMS[2]);

    constexpr int latBottomDMS[] = { 42, 15, 14 };
    const auto latBottomDecimal = nitf::Utils::geographicToDecimal(latBottomDMS[0], latBottomDMS[1], latBottomDMS[2]);

    constexpr int lonEastDMS[] = { -83, 42, 12 };
    const auto lonEastDecimal = nitf::Utils::geographicToDecimal(lonEastDMS[0], lonEastDMS[1], lonEastDMS[2]);

    constexpr int lonWestDMS[] = { -83, 45, 44 };
    const auto lonWestDecimal = nitf::Utils::geographicToDecimal(lonWestDMS[0], lonWestDMS[1], lonWestDMS[2]);

    double corners[4][2];
    corners[0][0] = latTopDecimal;     corners[0][1] = lonWestDecimal;
    corners[1][0] = latTopDecimal;     corners[1][1] = lonEastDecimal;
    corners[2][0] = latBottomDecimal;  corners[2][1] = lonEastDecimal;
    corners[3][0] = latBottomDecimal;  corners[3][1] = lonWestDecimal;

    header.setCornersFromLatLons(NITF_CORNERS_DECIMAL, corners);
}

static void addImageSegment(nitf::Record& record,
        size_t numRows,
        size_t numCols,
        size_t rowsPerBlock,
        size_t colsPerBlock,
        size_t bytesPerPixel,
        bool shouldCompress)
{
    nitf::ImageSegment segment = record.newImageSegment();
    nitf::ImageSubheader header = segment.getSubheader();

    header.getImageId().set("NITRO-TEST");
    header.getImageDateAndTime().set("20080812000000");

    if (shouldCompress)
    {
        header.getImageCompression().set("C8");
        header.getCompressionRate().set("N045");
    }

    /* Set the geo-corners to Ann Arbor, MI */
    setCornersFromDMSBox(header);

    std::vector<nitf::BandInfo> bands{ {nitf::Representation::R, nitf::Subcategory::None,
                   "N",       /* The band filter condition */
                   "   " } };    /* The band standard image filter code */


    const auto nbpp = gsl::narrow<uint32_t>(8 * bytesPerPixel);
    const auto abpp = gsl::narrow<uint32_t>(8 * bytesPerPixel);
    const auto iRep = nitf::ImageRepresentation::MONO;
    header.setPixelInformation(nitf::PixelValueType::Integer,
                               nbpp, /* Number of bits/pixel */
                               abpp, /* Actual number of bits/pixel */
                               "R",       /* Pixel justification */
                               iRep,      /* Image representation */
                               "VIS",     /* Image category */
                               bands);    /* Band information object list */



    /* for fun, let's add a comment */
    header.insertImageComment("NITF generated by NITRO", 0);
    header.setBlocking(gsl::narrow<uint32_t>(numRows),
                       gsl::narrow<uint32_t>(numCols),
                       gsl::narrow<uint32_t>(rowsPerBlock),
                       gsl::narrow<uint32_t>(colsPerBlock),
                       nitf::BlockingMode::Block);               /*!< Image mode */
}

// Make sure a file gets removed
struct EnsureFileCleanup final
{
    EnsureFileCleanup(const std::filesystem::path& pathname) : mPathname(pathname)
    {
        removeIfExists();
    }

    ~EnsureFileCleanup()
    {
        try
        {
            removeIfExists();
        }
        catch (...)
        {
        }
    }
    EnsureFileCleanup(const EnsureFileCleanup&) = default;
    EnsureFileCleanup& operator=(const EnsureFileCleanup&) = delete;

private:
    void removeIfExists()
    {
        if (exists(mPathname))
        {
            remove(mPathname);
        }
    }

    const std::filesystem::path mPathname;
};

struct CompareFiles final
{
    CompareFiles(const std::filesystem::path& lhsPathname)
    {
        readImage(lhsPathname, mLHS);
    }

    bool operator()(const std::string& prefix, const std::filesystem::path& rhsPathname) const
    {
        readImage(rhsPathname, mRHS);
        if (mLHS == mRHS)
        {
            std::clog << prefix << " matches\n";
            return true;
        }

        if (mLHS.size() != mRHS.size())
        {
            std::clog << prefix << " DOES NOT MATCH: file sizes are " << mLHS.size() << " vs. " << mRHS.size() << " bytes\n";
        }
        else
        {
            size_t ii;
            for (ii = 0; ii < mLHS.size(); ++ii)
            {
                if (mLHS[ii] != mRHS[ii])
                {
                    std::clog << prefix << " DOES NOT MATCH at byte " << ii << "\n";
                    break;
                }
            }
        }
        return false;
    }
private:
    std::vector<std::byte> mLHS;
    mutable std::vector<std::byte> mRHS;
    static void readImage(const std::filesystem::path& pathname, std::vector<std::byte>& data)
    {
        data.clear();
        nitf::Reader reader;
        nitf::IOHandle io(pathname.string());
        nitf::Record record = reader.read(io);
        nitf::ListIterator iter = record.getImages().begin();
        size_t image = 0;
        size_t imageOffset = 0;
        for (; iter != record.getImages().end(); ++iter)
        {
            nitf::ImageReader imageReader = reader.newImageReader(gsl::narrow<int>(image));
            nitf::ImageSegment imageSegment = *iter;
            nitf::ImageSubheader imageSubheader = imageSegment.getSubheader();
            const auto numBlocks = imageSubheader.numBlocksPerRow() * imageSubheader.numBlocksPerCol();
            const auto imageLength = imageSubheader.getNumBytesOfImageData();
            data.resize(imageOffset + imageLength);

            nitf::Uint64 blockOffset = 0;
            for (size_t block = 0; block < numBlocks; ++block)
            {
                // Read a block
                nitf::Uint64 bytesThisBlock(0);
                const auto blockData = imageReader.readBlock(gsl::narrow<uint32_t>(block), &bytesThisBlock);
                if (bytesThisBlock == 0)
                {
                    throw except::Exception(Ctxt("Failed to read block"));
                }

                // Copy it to the output
                memcpy(&data[imageOffset + blockOffset], blockData, bytesThisBlock);
                blockOffset += bytesThisBlock;
            }
            imageOffset += imageLength;
            ++image;
        }
    }
};

// Main test class
template <typename DataTypeT>
struct Tester final
{
    Tester(int numRowsPerBlock,
           size_t numColsPerBlock,
           bool setMaxProductSize,
           size_t maxRowsPerSegment) :
        mNormalPathname("normal_write.nitf"),
        mNormalFileCleanup(mNormalPathname),
        mSetMaxProductSize(setMaxProductSize),
        mMaxRowsPerSegment(maxRowsPerSegment),
        mTestPathname("streaming_write.nitf"),
        mSuccess(true)
    {
        // Generate test image
        const types::RowCol<size_t> globalImageDims(123, 56);
        mImage.resize(globalImageDims.area());
        srand(334);
        for (size_t ii = 0; ii < mImage.size(); ++ii)
        {
            mImage[ii] = static_cast<DataTypeT>(rand() % std::numeric_limits<DataTypeT>::max());
        }

        // Set segmenting dimensions
        mNumImages = 1;
        if (mSetMaxProductSize)
        {
            mNumImages = math::ceilingDivide(globalImageDims.row, mMaxRowsPerSegment);
        }

        mDims.resize(mNumImages);
        for (size_t ii = 0; ii < mNumImages; ++ii)
        {
            mDims[ii].col = globalImageDims.col;
            if (ii == mNumImages - 1)
            {
                mDims[ii].row = globalImageDims.row - ii * mMaxRowsPerSegment;
            }
            else
            {
                mDims[ii].row = mMaxRowsPerSegment;
            }
        }

        mBlockDims.row = (numRowsPerBlock == 0) ? mDims[0].row : numRowsPerBlock;
        mBlockDims.col = (numColsPerBlock == 0) ? mDims[0].col : numColsPerBlock;

        // Set up source data
        normalWrite();
        mCompareFiles.reset(new CompareFiles(mNormalPathname));

        // Pre-compress image data
        createCompressedImage();
    }
    Tester(const Tester&) = delete;
    Tester& operator=(const Tester&) = delete;
    Tester(Tester&&) = default;
    Tester& operator=(Tester&&) = delete;

    void testSingleWrite()
    {
        EnsureFileCleanup test(mTestPathname);
        auto record = populateRecord(mNormalPathname.string(), true /*shouldCompress*/, false /*normalWrite*/);

        std::vector<nitf::ByteProvider::PtrAndLength_t> desData;
        const nitf::CompressedByteProvider byteProvider(record, mBytesPerBlock, desData, mBlockDims.row, mBlockDims.col);

        size_t totalNumBytes = 0;
        for (size_t image = 0; image < mNumImages; ++image)
        {
            for (size_t block = 0; block < mBytesPerBlock[image].size(); ++block)
            {
                totalNumBytes += mBytesPerBlock[image][block];
            }
        }

        std::vector<std::byte> combinedCompressedBlocks(totalNumBytes);
        size_t offset = 0;
        size_t totalNumRows = 0;
        for (size_t image = 0; image < mNumImages; ++image)
        {
            totalNumRows += mDims[image].row;
            const auto& bytesPerBlock = mBytesPerBlock[image];
            for (size_t block = 0; block < bytesPerBlock.size(); ++block)
            {
                auto dest = &combinedCompressedBlocks[offset];
                const auto source = &(mCompressedBlocks[image][block][0]);
                memcpy(dest, source, bytesPerBlock[block]);
                offset += bytesPerBlock[block];
            }
        }

        nitf::Off fileOffset;
        nitf::NITFBufferList buffers;
        byteProvider.getBytes(&combinedCompressedBlocks[0], 0, totalNumRows, fileOffset, buffers);

        io::FileOutputStream outputStream(mTestPathname);
        const nitf::Off expectedNumBytes = byteProvider.getNumBytes(0, totalNumRows);
        write(fileOffset, buffers, expectedNumBytes, outputStream);

        compare("Single write");
    }

    void testMultipleWritesBlocked()
    {
        EnsureFileCleanup test(mTestPathname);
        auto record = populateRecord(mNormalPathname.string(), true /*shouldCompress*/, false /*normalWrite*/);

        std::vector<nitf::ByteProvider::PtrAndLength> desData;
        const nitf::CompressedByteProvider byteProvider(record, mBytesPerBlock, desData, mBlockDims.row, mBlockDims.col);

        io::FileOutputStream outputStream(mTestPathname);

        size_t startRow = 0;
        for (size_t image = 0; image < mNumImages; ++image)
        {
            const size_t blocksThisSegment = mBytesPerBlock[image].size();
            const size_t rowsLastBlock = mDims[image].row - mBlockDims.row * (blocksThisSegment - 1);

            for (size_t block = 0; block < blocksThisSegment; ++block)
            {
                const size_t numRows = (block == blocksThisSegment - 1) ? rowsLastBlock : mBlockDims.row;

                nitf::Off fileOffset;
                nitf::NITFBufferList buffers;
                byteProvider.getBytes(&(mCompressedBlocks[image][block][0]), startRow, numRows, fileOffset, buffers);
                if (image == 1)
                {
                    byteProvider.getNumBytes(startRow, numRows);
                }

                nitf::Off expectedNumBytes = byteProvider.getNumBytes(startRow, numRows);
                write(fileOffset, buffers, expectedNumBytes, outputStream);
                startRow += numRows;
            }
        }
        compare("Multiple writes blocked");
    }

    bool success() const
    {
        return mSuccess;
    }

private:
    void normalWrite() const
    {
        nitf::IOHandle handle(mNormalPathname.string(), NITF_ACCESS_WRITEONLY, NITF_CREATE);
        nitf::Writer writer;
        auto record = populateRecord(mNormalPathname.string(), false /*shouldCompress*/, true /*normalWrite*/);
        writer.prepare(handle, record);

        std::vector<nitf::ImageWriter> imageWriters;
        for (size_t ii = 0; ii < mNumImages; ++ii)
        {
            imageWriters.push_back(writer.newImageWriter(gsl::narrow<int>(ii)));
            const auto area = rowsToPixels(mDims[ii].row);
            const auto offset = rowsToPixels(ii * mMaxRowsPerSegment);
            nitf::BandSource bandSource = nitf::MemorySource(
                reinterpret_cast<const char*>(&mImage[0]),
                area, gsl::narrow<nitf::Off>(offset), sizeof(DataTypeT), 0);
            nitf::ImageSource imageSource;
            imageSource.addBand(bandSource);

            imageWriters[ii].setWriteCaching(1);
            imageWriters[ii].attachSource(imageSource);
        }
        writer.write();
    }

    nitf::Record populateRecord(
        const std::string& fileTitle,
        bool shouldCompress,
        bool /*normalWrite*/) const
    {
        nitf::Record retval;
        auto header = retval.getHeader();
        header.getOriginStationID().set("github.com");
        header.getFileTitle().set(fileTitle);

        for (size_t ii = 0; ii < mNumImages; ++ii)
        {
            addImageSegment(retval,
                mDims[ii].row, mDims[ii].col,
                mBlockDims.row, mBlockDims.col,
                sizeof(DataTypeT),
                shouldCompress);
        }
        return retval;
    }

    void createCompressedImage()
    {
        std::vector<size_t> rowsEachSegment(mNumImages, 0);
        for (size_t image = 0; image < mNumImages; ++image)
        {
            rowsEachSegment[image] = mDims[image].row;
        }
        nitf::ImageBlocker imageBlocker(
            rowsEachSegment, mDims[0].col, mBlockDims.row, mBlockDims.col);

        mBytesPerBlock.resize(mNumImages);
        for (size_t image = 0; image < mBytesPerBlock.size(); ++image)
        {
            const auto numBlocks = imageBlocker.getNumRowsOfBlocks(image) * imageBlocker.getNumColsOfBlocks();
            mBytesPerBlock[image].resize(numBlocks);
        }
        mCompressedBlocks.resize(mNumImages);

        for (size_t image = 0; image < mNumImages; ++image)
        {
            const size_t numBlocks = mBytesPerBlock[image].size();
            mCompressedBlocks[image].resize(numBlocks);

            compressImageSegment(
                image,
                imageBlocker,
                mCompressedBlocks[image],
                mBytesPerBlock[image]);
        }
    }

    void compressImageSegment(
        size_t imageNumber,
        const nitf::ImageBlocker& imageBlocker,
        std::vector<std::vector<std::byte>>& compressedBlocks,
        std::vector<size_t>& bytesPerBlock)
    {
        const types::RowCol<size_t>& imageDims = mDims[imageNumber];
        const j2k::CompressionParameters compressionParams(imageDims, mBlockDims, 1, 3);
        j2k::Compressor compressor(compressionParams);

        size_t rowsWritten = 0;
        for (size_t image = 0; image < imageNumber; ++image)
        {
            rowsWritten += mDims[image].row;
        }

        const auto imageStartRow = rowsWritten;
        for (size_t block = 0; block < bytesPerBlock.size(); ++block)
        {
            const size_t startRow = rowsWritten;
            const size_t pixelOffset = startRow * imageDims.col;
            const size_t lastRowThisImage = imageStartRow + imageDims.row;

            const auto numRows = std::min<size_t>(imageBlocker.getNumRowsPerBlock()[imageNumber], lastRowThisImage - startRow);

            const auto bytesInBlock = imageBlocker.getNumBytesRequired(startRow, numRows, sizeof(DataTypeT));
            std::vector<std::byte> blockData(bytesInBlock);
            imageBlocker.block(&mImage[pixelOffset], startRow, numRows, sizeof(DataTypeT), blockData.data());
            
            const std::span<const std::byte> blockData_(blockData.data(), blockData.size());
            compressor.compressTile(blockData_, block,
                compressedBlocks[block]);
            bytesPerBlock[block] = compressedBlocks[block].size();

            rowsWritten += numRows;
        }
    }

    void write(
        nitf::Off fileOffset,
        const nitf::NITFBufferList& buffers,
        nitf::Off computeNumBytes,
        io::FileOutputStream& outStream)
    {
        outStream.seek(fileOffset, io::Seekable::START);

        nitf::Off numBytes = 0;
        for (size_t ii = 0; ii < buffers.mBuffers.size(); ++ii)
        {
            outStream.write(
                static_cast<const std::byte*>(buffers.mBuffers[ii].mData),
                buffers.mBuffers[ii].mNumBytes);
            numBytes += buffers.mBuffers[ii].mNumBytes;
        }

        if (numBytes != computeNumBytes)
        {
            std::clog << "Computed " << computeNumBytes << " bytes but actually had " << numBytes << " bytes\n";
            mSuccess = false;
        }
    }


    size_t rowsToPixels(size_t rows) const noexcept
    {
        return rows * mDims[0].col * sizeof(DataTypeT);
    }

    std::string getSuffix() const
    {
        std::string suffix;
        if (mBlockDims.area() != 0 && mBlockDims != mDims[0])
        {
            suffix += " with blocking of rows/block=" + std::to_string(mBlockDims.row) + ", cols/block=" + std::to_string(mBlockDims.col);
        }
        return suffix;
    }

    void compare(const std::string& prefix)
    {
        std::string fullPrefix = prefix;
        if (mSetMaxProductSize)
        {
            fullPrefix += " (max rows per image " +  std::to_string(mMaxRowsPerSegment) + ")";
        }
        fullPrefix += getSuffix();
        if (!(*mCompareFiles)(fullPrefix, mTestPathname))
        {
            mSuccess = false;
        }
    }

private:
    const std::filesystem::path mNormalPathname;
    const EnsureFileCleanup mNormalFileCleanup;

    std::vector<types::RowCol<size_t> > mDims;
    types::RowCol<size_t> mBlockDims;
    const bool mSetMaxProductSize;
    const size_t mMaxRowsPerSegment;
    const std::filesystem::path mTestPathname;
    std::vector<DataTypeT> mImage;
    size_t mNumImages;

    std::vector<std::vector<std::vector<std::byte>>> mCompressedBlocks;
    std::vector<std::vector<size_t> > mBytesPerBlock;
    std::unique_ptr<const CompareFiles> mCompareFiles;
    bool mSuccess;
};

static Tester<uint8_t> make_Tester(bool setBlocking, std::optional<size_t> maxRowsPerSegment = std::optional<size_t>())
{
    // These intentionally do not divide evenly so there will be both pad rows and cols
    const auto numRowsPerBlock = setBlocking ? 40 : 0;
    constexpr size_t numColsPerBlock = 0;

    const auto setMaxProductSize = maxRowsPerSegment.has_value();
    const auto maxRowsPerSegment_ = setMaxProductSize ? *maxRowsPerSegment : 0;

    // Only 1 byte per pixel supported for now
    return Tester<uint8_t>(numRowsPerBlock, numColsPerBlock, setMaxProductSize, maxRowsPerSegment_);
}

TEST_CASE(j2k_compressed_byte_provider_maxRowsPerSegment0)
{
    sys::OS().setEnv("NITF_PLUGIN_PATH", nitf::Test::buildPluginsDir(), true /*overwrite*/);
    {
        auto tester = make_Tester(true /*setBlocking*/);
        tester.testMultipleWritesBlocked();
        TEST_ASSERT_TRUE(tester.success());
        tester.testSingleWrite();
        TEST_ASSERT_TRUE(tester.success());
    }
    {
        auto tester = make_Tester(false /*setBlocking*/);
        tester.testSingleWrite();
        TEST_ASSERT_TRUE(tester.success());
    }
}

TEST_CASE(j2k_compressed_byte_provider)
{
    sys::OS().setEnv("NITF_PLUGIN_PATH", nitf::Test::buildPluginsDir(), true /*overwrite*/);

    // Run tests forcing various numbers of segments
    // Blocking is set at 40 rows / block so can't go less than this
    // Actual limit is a bit higher, since j2k needs a minimum size
    const auto numRows = { 100, 80, 50 };
    for (auto maxRowsPerSegment_ : numRows)
    {
        const auto maxRowsPerSegment = gsl::narrow<size_t>(maxRowsPerSegment_);
        {
            auto tester = make_Tester(true /*setBlocking*/, maxRowsPerSegment);
            tester.testMultipleWritesBlocked();
            TEST_ASSERT_TRUE(tester.success());
            tester.testSingleWrite();
            TEST_ASSERT_TRUE(tester.success());
        }
        {
            auto tester = make_Tester(false /*setBlocking*/, maxRowsPerSegment);
            tester.testSingleWrite();
            TEST_ASSERT_TRUE(tester.success());
        }
    }
}

TEST_MAIN((void)argc;(void)argv;
//TEST_CHECK(j2k_compressed_byte_provider_maxRowsPerSegment0); // TODO: get working with CMake
//TEST_CHECK(j2k_compressed_byte_provider); // TODO: get working with CMake
)