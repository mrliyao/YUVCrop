#include <ostream>
#include <fstream>
#include <sys/stat.h>
// https://github.com/tanakh/cmdline
#include "../headers/cmdline.h"	

int main(int argc, char** argv)
{
	cmdline::parser argParser;

	// - input yuv cfg parse
	argParser.add<int>("width", 'w', "picture width", true);
	argParser.add<int>("height", 'h', "picture height", true);
	argParser.add<std::string>("raw_path", 'i', "raw sequence path", true);
	argParser.add<int>("raw_depth", 'd', "raw sequence bitdepth", true);
	// - crop yuv cfg parse
	argParser.add<std::string>("crop_path", 'o', "out crop sequence path", false);
	// - crop position cfg parse
	argParser.add<int>("poc", 'f', "poc of crop block: [0, num_frames-1]", false, 1, cmdline::range(0, 65535));
	argParser.add<int>("crop_x", 'x', "top left x of crop block", false, 0, cmdline::range(0, 65535));
	argParser.add<int>("crop_y", 'y', "top left y of crop block", false, 0, cmdline::range(0, 65535));
	argParser.add<int>("crop_w", 'c', "width(colum) of crop block", false, 128, cmdline::range(0, 65535));
	argParser.add<int>("crop_h", 'r', "height(roll) of crop block", false, 128, cmdline::range(0, 65535));

	argParser.parse_check(argc, argv);

	/*const std::string origYuvPath = "F:\\YUVs\\Mesh\\C4D4U\\softbody_C4D4U_01_P2_1920x1080_R30.yuv";
	const std::string predYuvPath = "F:\\Projects\\VTM\\Mesh\\AnchorModePrint\\VTM-12.0-ModePrint\\_test\\softbody_C4D4U_01_1920x1080_R30\\P2\\pred_1920x1080.yuv";
	const uint16_t picW = 1920, picH = 1080;*/

	// - input yuv cfg
	const uint16_t picW = argParser.get<int>("width");
	const uint16_t picH = argParser.get<int>("height");
	const std::string rawYuvPath = argParser.get<std::string>("raw_path");
	const int rawDepth = argParser.get<int>("raw_depth");
	// -- 8-bit depth or 10-bit depth only
	if (rawDepth != 8 && rawDepth != 10)
		throw std::runtime_error("not supported input yuv depth");

	const uint16_t DIV = 2;											// YUV420 only
	const uint16_t picSubW = picW / DIV, picSubH = picH / DIV;
	const uint16_t BITDEPTH_RAW = rawDepth == 8 ? 1 : 2;			// num of bytes per pel
	const uint32_t bytesPerLumaFrameOrig = picW * picH * BITDEPTH_RAW;
	const uint32_t bytesPerChromaFrameOrig = bytesPerLumaFrameOrig / (DIV * DIV);

	// -- num of total frames
	uint16_t numFrames = 0;
	struct _stat rawYuvInfo;
	if (_stat(rawYuvPath.c_str(), &rawYuvInfo))
		throw std::runtime_error("input yuv file open failed!");
	else
		numFrames = rawYuvInfo.st_size / (bytesPerLumaFrameOrig + 2 * bytesPerChromaFrameOrig);

	// - crop yuv cfg
	const uint16_t BITDEPTH_CROP = 1;								// output 8-bit(1-byte) crop yuv
	const uint16_t BITDEPTH_SHIFT = rawDepth == 8 ? 0 : 2;			// bitdepth shift
	const uint32_t bytesPerLumaFrameCrop = picW * picH * BITDEPTH_CROP;
	const uint32_t bytesPerChromaFramePred = bytesPerLumaFrameCrop / (DIV * DIV);

	// - crop position cfg
	const int cropPoc = argParser.get<int>("poc");
	if (cropPoc > numFrames - 1)
		throw std::runtime_error("crop poc greater than the total frames");
	const int cropX = argParser.get<int>("crop_x");
	const int cropY = argParser.get<int>("crop_y");
	const int cropW = argParser.get<int>("crop_w");
	const int cropH = argParser.get<int>("crop_h");
	const int cropSubW = cropW / DIV, cropSubH = cropH / DIV;

	if ((cropX + cropW > picW) || (cropY > cropH + picH))
		throw std::runtime_error("crop area out of sequence boundary!");

	std::string cropYuvPath = argParser.get<std::string>("crop_path");
	if (cropYuvPath.empty())
	{
		cropYuvPath = rawYuvPath;
		auto        insertIter = rawYuvPath.find(".yuv");
		std::string strInsert = "_crop_" + std::to_string(cropW) + "x" + std::to_string(cropH);
		cropYuvPath.insert(insertIter, strInsert);
	}


	// - croping
	std::ifstream rawYuvStrm(rawYuvPath, std::ios::in | std::ios::binary);
	std::ofstream cropYuvStrm(cropYuvPath, std::ios::app | std::ios::binary);

	// -- crop buf
	uint8_t* yCropBuf = new uint8_t[cropW * cropH];
	uint8_t* cbCropBuf = new uint8_t[cropSubW * cropSubH];
	uint8_t * crCropBuf = new uint8_t[cropSubW * cropSubH];

	// --read temp buf
	uint16_t* yTmpBuf = new uint16_t[picW * picH];
	uint16_t* cbTmpBuf = new uint16_t[picSubW * picSubH];
	uint16_t* crTmpBuf = new uint16_t[picSubW * picSubH];

	// -- move the stream pointer to target poc
	for (uint16_t poc = 0; poc != cropPoc; ++poc)
	{
		rawYuvStrm.read(reinterpret_cast<char*>(yTmpBuf), BITDEPTH_RAW * picW * picH);
		rawYuvStrm.read(reinterpret_cast<char*>(cbTmpBuf), BITDEPTH_RAW * picSubW * picSubH);
		rawYuvStrm.read(reinterpret_cast<char*>(crTmpBuf), BITDEPTH_RAW * picSubW * picSubH);
	}
	// -- store current frame data
	{
		rawYuvStrm.read(reinterpret_cast<char*>(yTmpBuf), BITDEPTH_RAW * picW * picH);
		rawYuvStrm.read(reinterpret_cast<char*>(cbTmpBuf), BITDEPTH_RAW * picSubW * picSubH);
		rawYuvStrm.read(reinterpret_cast<char*>(crTmpBuf), BITDEPTH_RAW * picSubW * picSubH);
	}

	// -- luma
	for (uint16_t i = 0; i != cropH; ++i)
	{
		for (uint16_t j = 0; j != cropW; ++j)
		{
			uint8_t* crop = yCropBuf + i * cropW + j;
			if (BITDEPTH_RAW == 1)
			{
				uint8_t* raw = (uint8_t*)yTmpBuf + (cropY + i) * picW + (cropX + j);
				*crop = (*raw) >> BITDEPTH_SHIFT;
			}				
			else
			{
				uint16_t* raw = (uint16_t*)yTmpBuf + (cropY + i) * picW + (cropX + j);
				*crop = (*raw) >> BITDEPTH_SHIFT;
			}		
		}
	}
	// -- chroma
	for (uint16_t i = 0; i != cropSubH; ++i)
	{
		for (uint16_t j = 0; j != cropSubW; ++j)
		{
			uint8_t* cbCrop = cbCropBuf + i * cropSubW + j;
			uint8_t* crCrop = crCropBuf + i * cropSubW + j;
			if (BITDEPTH_RAW == 1)
			{
				uint8_t* cbRaw = (uint8_t*)cbTmpBuf + (cropY / DIV + i) * picSubW + (cropX / DIV + j);
				uint8_t* crRaw = (uint8_t*)crTmpBuf + (cropY / DIV + i) * picSubW + (cropX / DIV + j);
				*cbCrop = (*cbRaw) >> BITDEPTH_SHIFT;
				*crCrop = (*crRaw) >> BITDEPTH_SHIFT;
			}
			else
			{
				uint16_t* cbRaw = (uint16_t*)cbTmpBuf + (cropY / DIV + i) * picSubW + (cropX / DIV + j);
				uint16_t* crRaw = (uint16_t*)crTmpBuf + (cropY / DIV + i) * picSubW + (cropX / DIV + j);
				*cbCrop = (*cbRaw) >> BITDEPTH_SHIFT;
				*crCrop = (*crRaw) >> BITDEPTH_SHIFT;
			}
		}
	}
	cropYuvStrm.write(reinterpret_cast<const char*>(yCropBuf), BITDEPTH_CROP * cropW * cropH);
	cropYuvStrm.write(reinterpret_cast<const char*>(cbCropBuf), BITDEPTH_CROP * cropSubW * cropSubH);
	cropYuvStrm.write(reinterpret_cast<const char*>(crCropBuf), BITDEPTH_CROP * cropSubW * cropSubH);

	delete[] yTmpBuf; delete[] cbTmpBuf; delete[] crTmpBuf;
	delete[] yCropBuf; delete[] cbCropBuf; delete[] crCropBuf;
}

