#pragma once

#include<iostream>
#include<format>
#include<algorithm>

#include<filesystem>
#include<chrono>
#include<mutex>
#include<semaphore>

#include<string>
#include<vector>
#include<unordered_set>
#include<unordered_map>

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::unordered_set;
using std::unordered_map;
using std::cout;
using std::endl;

#include "framework.h"
#include "resource.h"

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>

#include "libraw/libraw.h"
#include "libheif/heif.h"
#include "avif/avif.h"
#include "exiv2/exiv2.hpp"
#include "gif_lib.h"
#include "psapi.h"

struct rcFileInfo {
	uint8_t* addr = nullptr;
	size_t size = 0;
};

union intUnion {
	uint32_t u32;
	uint8_t u8[4];
	intUnion() :u32(0) {}
	intUnion(uint32_t n) :u32(n) {}
	intUnion(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
		u8[0] = b0;
		u8[1] = b1;
		u8[2] = b2;
		u8[3] = b3;
	}
	uint8_t& operator[](const int i) {
		return u8[i];
	}
	void operator=(const int i) {
		u32 = i;
	}
	void operator=(const uint32_t i) {
		u32 = i;
	}
	void operator=(const intUnion i) {
		u32 = i.u32;
	}
};

struct Cood {
	int x = 0;
	int y = 0;

	Cood operator+(const Cood& t) const {
		Cood temp;
		temp.x = this->x + t.x;
		temp.y = this->y + t.y;
		return temp;
	}

	Cood operator-(const Cood& t) const {
		Cood temp;
		temp.x = this->x - t.x;
		temp.y = this->y - t.y;
		return temp;
	}

	Cood operator*(int i) const {
		Cood temp;
		temp.x = this->x * i;
		temp.y = this->y * i;
		return temp;
	}

	Cood operator/(int i) const {
		Cood temp;
		temp.x = this->x / i;
		temp.y = this->y / i;
		return temp;
	}
};

struct ImageNode {
	cv::Mat img;
	int delay;
};
struct Frames {
	std::vector<ImageNode> imgList;
	string exifStr;
};

struct GifData {
	const unsigned char* m_lpBuffer;
	size_t m_nBufferSize;
	size_t m_nPosition;
};

namespace Utils {

	static const unordered_set<wstring> supportExt = {
		L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
		L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
		L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
		L".heic", L".heif", L".avif", L".avifs", L".gif",
	};

	static const unordered_set<wstring> supportRaw = {
		L".crw", L".cr2", L".cr3", // Canon
		L".nef", // Nikon
		L".arw", L".srf", L".sr2", // Sony
		L".pef", // Pentax
		L".orf", // Olympus
		L".rw2", // Panasonic
		L".raf", // Fujifilm
		L".kdc", // Kodak
		L".raw", L".dng", // Leica
		L".x3f", // Sigma
		L".mrw", // Minolta
		L".3fr"  // Hasselblad
	};

	static const bool TO_LOG_FILE = false;
	static FILE* fp = nullptr;

	template<typename... Args>
	static void log(const string_view fmt, Args&&... args) {
		auto ts = time(nullptr) + 8 * 3600ULL;// UTC+8
		int HH = (ts / 3600) % 24;
		int MM = (ts / 60) % 60;
		int SS = ts % 60;

		const string str = std::format("[{:02d}:{:02d}:{:02d}] ", HH, MM, SS) +
			std::vformat(fmt, std::make_format_args(args...)) + "\n";

		if (TO_LOG_FILE) {
			if (!fp) {
				fp = fopen("D:\\log.txt", "a");
				if (!fp)return;
			}
			fwrite(str.c_str(), 1, str.length(), fp);
			fflush(fp);
		}
		else {
			cout << str;
		}
	}

	string bin2Hex(const void* bytes, const size_t len) {
		auto charList = "0123456789ABCDEF";
		if (len == 0) return "";
		string res(len * 3 - 1, ' ');
		for (size_t i = 0; i < len; i++) {
			const uint8_t value = reinterpret_cast<const uint8_t*>(bytes)[i];
			res[i * 3] = charList[value >> 4];
			res[i * 3 + 1] = charList[value & 0x0f];
		}
		return res;
	}

	std::wstring ansiToWstring(const std::string& str) {
		if (str.empty())return L"";

		int wcharLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
		if (wcharLen == 0) return L"";

		std::wstring ret(wcharLen, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

		return ret;
	}

	std::string wstringToAnsi(const std::wstring& wstr) {
		if (wstr.empty())return "";

		int byteLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
		if (byteLen == 0) return "";

		std::string ret(byteLen, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
		return ret;
	}

	//UTF8 to UTF16
	std::wstring utf8ToWstring(const std::string& str) {
		if (str.empty())return L"";

		int wcharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
		if (wcharLen == 0) return L"";

		std::wstring ret(wcharLen, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

		return ret;
	}

	//UTF16 to UTF8
	std::string wstringToUtf8(const std::wstring& wstr) {
		if (wstr.empty())return "";

		int byteLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
		if (byteLen == 0) return "";

		std::string ret(byteLen, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
		return ret;
	}

	std::string utf8ToAnsi(const std::string& str) {
		return wstringToAnsi(utf8ToWstring(str));
	}

	std::string ansiToUtf8(const std::string& str) {
		return wstringToUtf8(ansiToWstring(str));
	}

	rcFileInfo GetResource(unsigned int idi, const wchar_t* type) {
		rcFileInfo rc;

		HMODULE ghmodule = GetModuleHandle(NULL);
		if (ghmodule == NULL) {
			log("ghmodule null");
			return rc;
		}

		HRSRC hrsrc = FindResource(ghmodule, MAKEINTRESOURCE(idi), type);
		if (hrsrc == NULL) {
			log("hrstc null");
			return rc;
		}

		HGLOBAL hg = LoadResource(ghmodule, hrsrc);
		if (hg == NULL) {
			log("hg null");
			return rc;
		}

		rc.addr = (unsigned char*)(LockResource(hg));
		rc.size = SizeofResource(ghmodule, hrsrc);
		return rc;
	}

	string size2Str(const int fileSize) {
		if (fileSize < 1024) return std::format("{} Bytes", fileSize);
		if (fileSize < 1024 * 1024) return std::format("{:.1f} KiB", fileSize / 1024.0);
		if (fileSize < 1024 * 1024 * 1024) return std::format("{:.1f} MiB", fileSize / (1024.0 * 1024));
		return std::format("{:.1f} GiB", fileSize / (1024.0 * 1024 * 1024));
	}

	string timeStamp2Str(time_t timeStamp) {
		timeStamp += 8ULL * 3600; // UTC+8
		std::tm* ptm = std::gmtime(&timeStamp);
		std::stringstream ss;
		ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S UTC+8");
		return ss.str();
	}

	std::string getSimpleInfo(const wstring& path, int width, int height, const uint8_t* buf, int fileSize) {
		return std::format("路径: {}\n大小: {}\n分辨率: {}x{}",
			wstringToUtf8(path),
			size2Str(fileSize),
			width, height);
	}

	// https://exiv2.org/tags.html
	// https://www.colorpilot.com/exiftable-thumbnail.html
	const std::unordered_map<std::string, std::string> exifTagsMap = {
		{"Exif.Image.0x9a00", "设备"},
		{"Exif.Image.ProcessingSoftware", "处理软件"},
		{"Exif.Image.NewSubfileType", "新子文件类型"},
		{"Exif.Image.SubfileType", "子文件类型"},
		{"Exif.Image.ImageWidth", "图像宽度"},
		{"Exif.Image.ImageLength", "图像长度"},
		{"Exif.Image.BitsPerSample", "每个样本的位数"},
		{"Exif.Image.Compression", "压缩"},
		{"Exif.Image.PhotometricInterpretation", "光度解释"},
		{"Exif.Image.Thresholding", "阈值处理"},
		{"Exif.Image.CellWidth", "单元格宽度"},
		{"Exif.Image.CellLength", "单元格长度"},
		{"Exif.Image.FillOrder", "填充顺序"},
		{"Exif.Image.DocumentName", "文档名称"},
		{"Exif.Image.ImageDescription", "图像描述"},
		{"Exif.Image.Make", "制造商"},
		{"Exif.Image.Model", "型号"},
		{"Exif.Image.StripOffsets", "条带偏移量"},
		{"Exif.Image.Orientation", "方向"},
		{"Exif.Image.SamplesPerPixel", "每像素样本数"},
		{"Exif.Image.RowsPerStrip", "每条带行数"},
		{"Exif.Image.StripByteCounts", "条带字节数"},
		{"Exif.Image.XResolution", "X分辨率"},
		{"Exif.Image.YResolution", "Y分辨率"},
		{"Exif.Image.PlanarConfiguration", "平面配置"},
		{"Exif.Image.PageName", "页面名称"},
		{"Exif.Image.XPosition", "X位置"},
		{"Exif.Image.YPosition", "Y位置"},
		{"Exif.Image.GrayResponseUnit", "灰度响应单位"},
		{"Exif.Image.GrayResponseCurve", "灰度响应曲线"},
		{"Exif.Image.T4Options", "T4选项"},
		{"Exif.Image.T6Options", "T6选项"},
		{"Exif.Image.ResolutionUnit", "分辨率单位"},
		{"Exif.Image.PageNumber", "页码"},
		{"Exif.Image.TransferFunction", "传输函数"},
		{"Exif.Image.Software", "软件"},
		{"Exif.Image.DateTime", "日期时间"},
		{"Exif.Image.Artist", "艺术家"},
		{"Exif.Image.HostComputer", "主机计算机"},
		{"Exif.Image.Predictor", "预测器"},
		{"Exif.Image.WhitePoint", "白点"},
		{"Exif.Image.PrimaryChromaticities", "原色色度"},
		{"Exif.Image.ColorMap", "颜色映射"},
		{"Exif.Image.HalftoneHints", "半色调提示"},
		{"Exif.Image.TileWidth", "瓦片宽度"},
		{"Exif.Image.TileLength", "瓦片长度"},
		{"Exif.Image.TileOffsets", "瓦片偏移量"},
		{"Exif.Image.TileByteCounts", "瓦片字节数"},
		{"Exif.Image.SubIFDs", "子IFDs"},
		{"Exif.Image.InkSet", "墨水集"},
		{"Exif.Image.InkNames", "墨水名称"},
		{"Exif.Image.NumberOfInks", "墨水数量"},
		{"Exif.Image.DotRange", "点范围"},
		{"Exif.Image.TargetPrinter", "目标打印机"},
		{"Exif.Image.ExtraSamples", "额外样本"},
		{"Exif.Image.SampleFormat", "样本格式"},
		{"Exif.Image.SMinSampleValue", "最小样本值"},
		{"Exif.Image.SMaxSampleValue", "最大样本值"},
		{"Exif.Image.TransferRange", "传输范围"},
		{"Exif.Image.ClipPath", "裁剪路径"},
		{"Exif.Image.XClipPathUnits", "X裁剪路径单位"},
		{"Exif.Image.YClipPathUnits", "Y裁剪路径单位"},
		{"Exif.Image.Indexed", "索引"},
		{"Exif.Image.JPEGTables", "JPEG表"},
		{"Exif.Image.OPIProxy", "OPI代理"},
		{"Exif.Image.JPEGProc", "JPEG处理"},
		{"Exif.Image.JPEGInterchangeFormat", "JPEG交换格式"},
		{"Exif.Image.JPEGInterchangeFormatLength", "JPEG交换格式长度"},
		{"Exif.Image.JPEGRestartInterval", "JPEG重启间隔"},
		{"Exif.Image.JPEGLosslessPredictors", "JPEG无损预测器"},
		{"Exif.Image.JPEGPointTransforms", "JPEG点变换"},
		{"Exif.Image.JPEGQTables", "JPEG量化表"},
		{"Exif.Image.JPEGDCTables", "JPEG直流表"},
		{"Exif.Image.JPEGACTables", "JPEG交流表"},
		{"Exif.Image.YCbCrCoefficients", "YCbCr系数"},
		{"Exif.Image.YCbCrSubSampling", "YCbCr子采样"},
		{"Exif.Image.YCbCrPositioning", "YCbCr定位"},
		{"Exif.Image.ReferenceBlackWhite", "参考黑白点"},
		{"Exif.Image.XMLPacket", "XML数据包"},
		{"Exif.Image.Rating", "评分"},
		{"Exif.Image.RatingPercent", "评分百分比"},
		{"Exif.Image.VignettingCorrParams", "暗角校正参数"},
		{"Exif.Image.ChromaticAberrationCorrParams", "色差校正参数"},
		{"Exif.Image.DistortionCorrParams", "畸变校正参数"},
		{"Exif.Image.ImageID", "图像ID"},
		{"Exif.Image.CFARepeatPatternDim", "CFA重复模式尺寸"},
		{"Exif.Image.CFAPattern", "CFA模式"},
		{"Exif.Image.BatteryLevel", "电池电量"},
		{"Exif.Image.Copyright", "版权"},
		{"Exif.Image.ExposureTime", "曝光时间"},
		{"Exif.Image.FNumber", "光圈值"},
		{"Exif.Image.IPTCNAA", "IPTC/NAA"},
		{"Exif.Image.ImageResources", "图像资源"},
		{"Exif.Image.ExifTag", "Exif标签"},
		{"Exif.Image.InterColorProfile", "内部颜色配置文件"},
		{"Exif.Image.ExposureProgram", "曝光程序"},
		{"Exif.Image.SpectralSensitivity", "光谱敏感度"},
		{"Exif.Image.GPSTag", "GPS标签"},
		{"Exif.Image.ISOSpeedRatings", "ISO速度等级"},
		{"Exif.Image.OECF", "光电转换函数"},
		{"Exif.Image.Interlace", "隔行扫描"},
		{"Exif.Image.TimeZoneOffset", "时区偏移"},
		{"Exif.Image.SelfTimerMode", "自拍模式"},
		{"Exif.Image.DateTimeOriginal", "原始日期时间"},
		{"Exif.Image.CompressedBitsPerPixel", "每像素压缩位数"},
		{"Exif.Image.ShutterSpeedValue", "快门速度值"},
		{"Exif.Image.ApertureValue", "光圈值"},
		{"Exif.Image.BrightnessValue", "亮度值"},
		{"Exif.Image.ExposureBiasValue", "曝光补偿值"},
		{"Exif.Image.MaxApertureValue", "最大光圈值"},
		{"Exif.Image.SubjectDistance", "主体距离"},
		{"Exif.Image.MeteringMode", "测光模式"},
		{"Exif.Image.LightSource", "光源"},
		{"Exif.Image.Flash", "闪光灯"},
		{"Exif.Image.FocalLength", "焦距"},
		{"Exif.Image.FlashEnergy", "闪光灯能量"},
		{"Exif.Image.SpatialFrequencyResponse", "空间频率响应"},
		{"Exif.Image.Noise", "噪声"},
		{"Exif.Image.FocalPlaneXResolution", "焦平面X分辨率"},
		{"Exif.Image.FocalPlaneYResolution", "焦平面Y分辨率"},
		{"Exif.Image.FocalPlaneResolutionUnit", "焦平面分辨率单位"},
		{"Exif.Image.ImageNumber", "图像编号"},
		{"Exif.Image.SecurityClassification", "安全分类"},
		{"Exif.Image.ImageHistory", "图像历史"},
		{"Exif.Image.SubjectLocation", "主体位置"},
		{"Exif.Image.ExposureIndex", "曝光指数"},
		{"Exif.Image.TIFFEPStandardID", "TIFF/EP标准ID"},
		{"Exif.Image.SensingMethod", "感应方法"},
		{"Exif.Image.XPTitle", "XP标题"},
		{"Exif.Image.XPComment", "XP注释"},
		{"Exif.Image.XPAuthor", "XP作者"},
		{"Exif.Image.XPKeywords", "XP关键词"},
		{"Exif.Image.XPSubject", "XP主题"},
		{"Exif.Image.PrintImageMatching", "打印图像匹配"},
		{"Exif.Image.DNGVersion", "DNG版本"},
		{"Exif.Image.DNGBackwardVersion", "DNG向后兼容版本"},
		{"Exif.Image.UniqueCameraModel", "唯一相机型号"},
		{"Exif.Image.LocalizedCameraModel", "本地化相机型号"},
		{"Exif.Image.CFAPlaneColor", "CFA平面颜色"},
		{"Exif.Image.CFALayout", "CFA布局"},
		{"Exif.Image.LinearizationTable", "线性化表"},
		{"Exif.Image.BlackLevelRepeatDim", "黑电平重复尺寸"},
		{"Exif.Image.BlackLevel", "黑电平"},
		{"Exif.Image.BlackLevelDeltaH", "水平黑电平增量"},
		{"Exif.Image.BlackLevelDeltaV", "垂直黑电平增量"},
		{"Exif.Image.WhiteLevel", "白电平"},
		{"Exif.Image.DefaultScale", "默认比例"},
		{"Exif.Image.DefaultCropOrigin", "默认裁剪原点"},
		{"Exif.Image.DefaultCropSize", "默认裁剪尺寸"},
		{"Exif.Image.ColorMatrix1", "颜色矩阵1"},
		{"Exif.Image.ColorMatrix2", "颜色矩阵2"},
		{"Exif.Image.CameraCalibration1", "相机校准1"},
		{"Exif.Image.CameraCalibration2", "相机校准2"},
		{"Exif.Image.ReductionMatrix1", "缩减矩阵1"},
		{"Exif.Image.ReductionMatrix2", "缩减矩阵2"},
		{"Exif.Image.AnalogBalance", "模拟平衡"},
		{"Exif.Image.AsShotNeutral", "拍摄时的中性点"},
		{"Exif.Image.AsShotWhiteXY", "拍摄时的白点XY坐标"},
		{"Exif.Image.BaselineExposure", "基准曝光"},
		{"Exif.Image.BaselineNoise", "基准噪声"},
		{"Exif.Image.BaselineSharpness", "基准锐度"},
		{"Exif.Image.BayerGreenSplit", "拜耳绿色分离"},
		{"Exif.Image.LinearResponseLimit", "线性响应限制"},
		{"Exif.Image.CameraSerialNumber", "相机序列号"},
		{"Exif.Image.LensInfo", "镜头信息"},
		{"Exif.Image.ChromaBlurRadius", "色度模糊半径"},
		{"Exif.Image.AntiAliasStrength", "抗锯齿强度"},
		{"Exif.Image.ShadowScale", "阴影比例"},
		{"Exif.Image.DNGPrivateData", "DNG私有数据"},
		{"Exif.Image.CalibrationIlluminant1", "校准光源1"},
		{"Exif.Image.CalibrationIlluminant2", "校准光源2"},
		{"Exif.Image.BestQualityScale", "最佳质量比例"},
		{"Exif.Image.RawDataUniqueID", "原始数据唯一ID"},
		{"Exif.Image.OriginalRawFileName", "原始RAW文件名"},
		{"Exif.Image.OriginalRawFileData", "原始RAW文件数据"},
		{"Exif.Image.ActiveArea", "有效区域"},
		{"Exif.Image.MaskedAreas", "遮罩区域"},
		{"Exif.Image.AsShotICCProfile", "拍摄时的ICC配置文件"},
		{"Exif.Image.AsShotPreProfileMatrix", "拍摄时的预配置文件矩阵"},
		{"Exif.Image.CurrentICCProfile", "当前ICC配置文件"},
		{"Exif.Image.CurrentPreProfileMatrix", "当前预配置文件矩阵"},
		{"Exif.Image.ColorimetricReference", "比色参考"},
		{"Exif.Image.CameraCalibrationSignature", "相机校准签名"},
		{"Exif.Image.ProfileCalibrationSignature", "配置文件校准签名"},
		{"Exif.Image.ExtraCameraProfiles", "额外相机配置文件"},
		{"Exif.Image.AsShotProfileName", "拍摄时配置文件名称"},
		{"Exif.Image.NoiseReductionApplied", "已应用的降噪"},
		{"Exif.Image.ProfileName", "配置文件名称"},
		{"Exif.Image.ProfileHueSatMapDims", "配置文件色相饱和度映射维度"},
		{"Exif.Image.ProfileHueSatMapData1", "配置文件色相饱和度映射数据1"},
		{"Exif.Image.ProfileHueSatMapData2", "配置文件色相饱和度映射数据2"},
		{"Exif.Image.ProfileToneCurve", "配置文件色调曲线"},
		{"Exif.Image.ProfileEmbedPolicy", "配置文件嵌入策略"},
		{"Exif.Image.ProfileCopyright", "配置文件版权"},
		{"Exif.Image.ForwardMatrix1", "前向矩阵1"},
		{"Exif.Image.ForwardMatrix2", "前向矩阵2"},
		{"Exif.Image.PreviewApplicationName", "预览应用程序名称"},
		{"Exif.Image.PreviewApplicationVersion", "预览应用程序版本"},
		{"Exif.Image.PreviewSettingsName", "预览设置名称"},
		{"Exif.Image.PreviewSettingsDigest", "预览设置摘要"},
		{"Exif.Image.PreviewColorSpace", "预览色彩空间"},
		{"Exif.Image.PreviewDateTime", "预览日期时间"},
		{"Exif.Image.RawImageDigest", "原始图像摘要"},
		{"Exif.Image.OriginalRawFileDigest", "原始RAW文件摘要"},
		{"Exif.Image.SubTileBlockSize", "子块大小"},
		{"Exif.Image.RowInterleaveFactor", "行交错因子"},
		{"Exif.Image.ProfileLookTableDims", "配置文件查找表维度"},
		{"Exif.Image.ProfileLookTableData", "配置文件查找表数据"},
		{"Exif.Image.OpcodeList1", "操作码列表1"},
		{"Exif.Image.OpcodeList2", "操作码列表2"},
		{"Exif.Image.OpcodeList3", "操作码列表3"},
		{"Exif.Image.NoiseProfile", "噪声配置文件"},
		{"Exif.Image.TimeCodes", "时间码"},
		{"Exif.Image.FrameRate", "帧率"},
		{"Exif.Image.TStop", "T档"},
		{"Exif.Image.ReelName", "胶片名称"},
		{"Exif.Image.CameraLabel", "相机标签"},
		{"Exif.Image.OriginalDefaultFinalSize", "原始默认最终尺寸"},
		{"Exif.Image.OriginalBestQualityFinalSize", "原始最佳质量最终尺寸"},
		{"Exif.Image.OriginalDefaultCropSize", "原始默认裁剪尺寸"},
		{"Exif.Image.ProfileHueSatMapEncoding", "配置文件色相饱和度映射编码"},
		{"Exif.Image.ProfileLookTableEncoding", "配置文件查找表编码"},
		{"Exif.Image.BaselineExposureOffset", "基准曝光偏移"},
		{"Exif.Image.DefaultBlackRender", "默认黑色渲染"},
		{"Exif.Image.NewRawImageDigest", "新原始图像摘要"},
		{"Exif.Image.RawToPreviewGain", "原始到预览增益"},
		{"Exif.Image.DefaultUserCrop", "默认用户裁剪"},
		{"Exif.Image.DepthFormat", "深度格式"},
		{"Exif.Image.DepthNear", "近深度"},
		{"Exif.Image.DepthFar", "远深度"},
		{"Exif.Image.DepthUnits", "深度单位"},
		{"Exif.Image.DepthMeasureType", "深度测量类型"},
		{"Exif.Image.EnhanceParams", "增强参数"},
		{"Exif.Image.ProfileGainTableMap", "配置文件增益表映射"},
		{"Exif.Image.SemanticName", "语义名称"},
		{"Exif.Image.SemanticInstanceID", "语义实例ID"},
		{"Exif.Image.CalibrationIlluminant3", "校准光源3"},
		{"Exif.Image.CameraCalibration3", "相机校准3"},
		{"Exif.Image.ColorMatrix3", "颜色矩阵3"},
		{"Exif.Image.ForwardMatrix3", "前向矩阵3"},
		{"Exif.Image.IlluminantData1", "光源数据1"},
		{"Exif.Image.IlluminantData2", "光源数据2"},
		{"Exif.Image.IlluminantData3", "光源数据3"},
		{"Exif.Image.MaskSubArea", "遮罩子区域"},
		{"Exif.Image.ProfileHueSatMapData3", "配置文件色相饱和度映射数据3"},
		{"Exif.Image.ReductionMatrix3", "缩减矩阵3"},
		{"Exif.Image.RGBTables", "RGB表"},
		{"Exif.Image.ProfileGainTableMap2", "配置文件增益表映射2"},
		{"Exif.Image.ColumnInterleaveFactor", "列交错因子"},
		{"Exif.Image.ImageSequenceInfo", "图像序列信息"},
		{"Exif.Image.ImageStats", "图像统计"},
		{"Exif.Image.ProfileDynamicRange", "配置文件动态范围"},
		{"Exif.Image.ProfileGroupName", "配置文件组名称"},
		{"Exif.Image.JXLDistance", "JXL距离"},
		{"Exif.Image.JXLEffort", "JXL努力程度"},
		{"Exif.Image.JXLDecodeSpeed", "JXL解码速度"},
		{"Exif.Photo.ExposureTime", "曝光时间"},
		{"Exif.Photo.FNumber", "光圈值"},
		{"Exif.Photo.ExposureProgram", "曝光程序"},
		{"Exif.Photo.SpectralSensitivity", "光谱敏感度"},
		{"Exif.Photo.ISOSpeedRatings", "ISO感光度"},
		{"Exif.Photo.OECF", "光电转换函数"},
		{"Exif.Photo.SensitivityType", "感光度类型"},
		{"Exif.Photo.StandardOutputSensitivity", "标准输出感光度"},
		{"Exif.Photo.RecommendedExposureIndex", "推荐曝光指数"},
		{"Exif.Photo.ISOSpeed", "ISO速度"},
		{"Exif.Photo.ISOSpeedLatitudeyyy", "ISO速度纬度yyy"},
		{"Exif.Photo.ISOSpeedLatitudezzz", "ISO速度纬度zzz"},
		{"Exif.Photo.ExifVersion", "Exif版本"},
		{"Exif.Photo.DateTimeOriginal", "原始日期时间"},
		{"Exif.Photo.DateTimeDigitized", "数字日期时间"},
		{"Exif.Photo.OffsetTime", "偏移时间"},
		{"Exif.Photo.OffsetTimeOriginal", "原始偏移时间"},
		{"Exif.Photo.OffsetTimeDigitized", "数字偏移时间"},
		{"Exif.Photo.ComponentsConfiguration", "组件配置"},
		{"Exif.Photo.CompressedBitsPerPixel", "每像素压缩位数"},
		{"Exif.Photo.ShutterSpeedValue", "快门速度值"},
		{"Exif.Photo.ApertureValue", "光圈值"},
		{"Exif.Photo.BrightnessValue", "亮度值"},
		{"Exif.Photo.ExposureBiasValue", "曝光补偿值"},
		{"Exif.Photo.MaxApertureValue", "最大光圈值"},
		{"Exif.Photo.SubjectDistance", "主体距离"},
		{"Exif.Photo.MeteringMode", "测光模式"},
		{"Exif.Photo.LightSource", "光源"},
		{"Exif.Photo.Flash", "闪光灯"},
		{"Exif.Photo.FocalLength", "焦距"},
		{"Exif.Photo.SubjectArea", "主体区域"},
		{"Exif.Photo.UserComment", "用户评论"},
		{"Exif.Photo.SubSecTime", "亚秒时间"},
		{"Exif.Photo.SubSecTimeOriginal", "原始亚秒时间"},
		{"Exif.Photo.SubSecTimeDigitized", "数字亚秒时间"},
		{"Exif.Photo.Temperature", "温度"},
		{"Exif.Photo.Humidity", "湿度"},
		{"Exif.Photo.Pressure", "压力"},
		{"Exif.Photo.WaterDepth", "水深"},
		{"Exif.Photo.Acceleration", "加速度"},
		{"Exif.Photo.CameraElevationAngle", "相机仰角"},
		{"Exif.Photo.FlashpixVersion", "Flashpix版本"},
		{"Exif.Photo.ColorSpace", "色彩空间"},
		{"Exif.Photo.PixelXDimension", "像素X维度"},
		{"Exif.Photo.PixelYDimension", "像素Y维度"},
		{"Exif.Photo.RelatedSoundFile", "相关音频文件"},
		{"Exif.Photo.InteroperabilityTag", "互操作性标签"},
		{"Exif.Photo.FlashEnergy", "闪光灯能量"},
		{"Exif.Photo.SpatialFrequencyResponse", "空间频率响应"},
		{"Exif.Photo.FocalPlaneXResolution", "焦平面X分辨率"},
		{"Exif.Photo.FocalPlaneYResolution", "焦平面Y分辨率"},
		{"Exif.Photo.FocalPlaneResolutionUnit", "焦平面分辨率单位"},
		{"Exif.Photo.SubjectLocation", "主体位置"},
		{"Exif.Photo.ExposureIndex", "曝光指数"},
		{"Exif.Photo.SensingMethod", "感应方法"},
		{"Exif.Photo.FileSource", "文件源"},
		{"Exif.Photo.SceneType", "场景类型"},
		{"Exif.Photo.CFAPattern", "CFA模式"},
		{"Exif.Photo.CustomRendered", "自定义渲染"},
		{"Exif.Photo.ExposureMode", "曝光模式"},
		{"Exif.Photo.WhiteBalance", "白平衡"},
		{"Exif.Photo.DigitalZoomRatio", "数字变焦比例"},
		{"Exif.Photo.FocalLengthIn35mmFilm", "35mm胶片等效焦距"},
		{"Exif.Photo.SceneCaptureType", "场景拍摄类型"},
		{"Exif.Photo.GainControl", "增益控制"},
		{"Exif.Photo.Contrast", "对比度"},
		{"Exif.Photo.Saturation", "饱和度"},
		{"Exif.Photo.Sharpness", "锐度"},
		{"Exif.Photo.DeviceSettingDescription", "设备设置描述"},
		{"Exif.Photo.SubjectDistanceRange", "主体距离范围"},
		{"Exif.Photo.ImageUniqueID", "图像唯一ID"},
		{"Exif.Photo.CameraOwnerName", "相机所有者名称"},
		{"Exif.Photo.BodySerialNumber", "机身序列号"},
		{"Exif.Photo.LensSpecification", "镜头规格"},
		{"Exif.Photo.LensMake", "镜头制造商"},
		{"Exif.Photo.LensModel", "镜头型号"},
		{"Exif.Photo.LensSerialNumber", "镜头序列号"},
		{"Exif.Photo.ImageTitle", "图像标题"},
		{"Exif.Photo.Photographer", "摄影师"},
		{"Exif.Photo.ImageEditor", "图像编辑器"},
		{"Exif.Photo.CameraFirmware", "相机固件"},
		{"Exif.Photo.RAWDevelopingSoftware", "RAW处理软件"},
		{"Exif.Photo.ImageEditingSoftware", "图像编辑软件"},
		{"Exif.Photo.MetadataEditingSoftware", "元数据编辑软件"},
		{"Exif.Photo.CompositeImage", "合成图像"},
		{"Exif.Photo.SourceImageNumberOfCompositeImage", "合成图像的源图像数量"},
		{"Exif.Photo.SourceExposureTimesOfCompositeImage", "合成图像的源曝光时间"},
		{"Exif.Photo.Gamma", "伽马值"},
		{"Exif.Iop.InteroperabilityIndex", "互操作性索引"},
		{"Exif.Iop.InteroperabilityVersion", "互操作性版本"},
		{"Exif.Iop.RelatedImageFileFormat", "相关图像文件格式"},
		{"Exif.Iop.RelatedImageWidth", "相关图像宽度"},
		{"Exif.Iop.RelatedImageLength", "相关图像长度"},
		{"Exif.GPSInfo.GPSVersionID", "GPS版本ID"},
		{"Exif.GPSInfo.GPSLatitudeRef", "GPS纬度参考"},
		{"Exif.GPSInfo.GPSLatitude", "GPS纬度"},
		{"Exif.GPSInfo.GPSLongitudeRef", "GPS经度参考"},
		{"Exif.GPSInfo.GPSLongitude", "GPS经度"},
		{"Exif.GPSInfo.GPSAltitudeRef", "GPS高度参考"},
		{"Exif.GPSInfo.GPSAltitude", "GPS高度"},
		{"Exif.GPSInfo.GPSTimeStamp", "GPS时间戳"},
		{"Exif.GPSInfo.GPSSatellites", "GPS卫星数"},
		{"Exif.GPSInfo.GPSStatus", "GPS状态"},
		{"Exif.GPSInfo.GPSMeasureMode", "GPS测量模式"},
		{"Exif.GPSInfo.GPSDOP", "GPS精度因子"},
		{"Exif.GPSInfo.GPSSpeedRef", "GPS速度参考"},
		{"Exif.GPSInfo.GPSSpeed", "GPS速度"},
		{"Exif.GPSInfo.GPSTrackRef", "GPS方向参考"},
		{"Exif.GPSInfo.GPSTrack", "GPS方向"},
		{"Exif.GPSInfo.GPSImgDirectionRef", "GPS图像方向参考"},
		{"Exif.GPSInfo.GPSImgDirection", "GPS图像方向"},
		{"Exif.GPSInfo.GPSMapDatum", "GPS地图基准"},
		{"Exif.GPSInfo.GPSDestLatitudeRef", "GPS目的地纬度参考"},
		{"Exif.GPSInfo.GPSDestLatitude", "GPS目的地纬度"},
		{"Exif.GPSInfo.GPSDestLongitudeRef", "GPS目的地经度参考"},
		{"Exif.GPSInfo.GPSDestLongitude", "GPS目的地经度"},
		{"Exif.GPSInfo.GPSDestBearingRef", "GPS目的地方位参考"},
		{"Exif.GPSInfo.GPSDestBearing", "GPS目的地方位"},
		{"Exif.GPSInfo.GPSDestDistanceRef", "GPS目的地距离参考"},
		{"Exif.GPSInfo.GPSDestDistance", "GPS目的地距离"},
		{"Exif.GPSInfo.GPSProcessingMethod", "GPS处理方法"},
		{"Exif.GPSInfo.GPSAreaInformation", "GPS区域信息"},
		{"Exif.GPSInfo.GPSDateStamp", "GPS日期戳"},
		{"Exif.GPSInfo.GPSDifferential", "GPS差分校正"},
		{"Exif.GPSInfo.GPSHPositioningError", "GPS水平定位误差"},
		{"Exif.MpfInfo.MPFVersion", "MPF版本"},
		{"Exif.MpfInfo.MPFNumberOfImages", "MPF图像数量"},
		{"Exif.MpfInfo.MPFImageList", "MPF图像列表"},
		{"Exif.MpfInfo.MPFImageUIDList", "MPF图像UID列表"},
		{"Exif.MpfInfo.MPFTotalFrames", "MPF总帧数"},
		{"Exif.MpfInfo.MPFIndividualNum", "MPF个体编号"},
		{"Exif.MpfInfo.MPFPanOrientation", "MPF全景方向"},
		{"Exif.MpfInfo.MPFPanOverlapH", "MPF水平重叠"},
		{"Exif.MpfInfo.MPFPanOverlapV", "MPF垂直重叠"},
		{"Exif.MpfInfo.MPFBaseViewpointNum", "MPF基准视点编号"},
		{"Exif.MpfInfo.MPFConvergenceAngle", "MPF会聚角"},
		{"Exif.MpfInfo.MPFBaselineLength", "MPF基线长度"},
		{"Exif.MpfInfo.MPFVerticalDivergence", "MPF垂直偏差"},
		{"Exif.MpfInfo.MPFAxisDistanceX", "MPF X轴距离"},
		{"Exif.MpfInfo.MPFAxisDistanceY", "MPF Y轴距离"},
		{"Exif.MpfInfo.MPFAxisDistanceZ", "MPF Z轴距离"},
		{"Exif.MpfInfo.MPFYawAngle", "MPF偏航角"},
		{"Exif.MpfInfo.MPFPitchAngle", "MPF俯仰角"},
		{"Exif.MpfInfo.MPFRollAngle", "MPF翻滚角"},
		{ "Exif.Thumbnail.ActiveArea", "缩略图.有效区域" },
		{ "Exif.Thumbnail.AnalogBalance", "缩略图.模拟平衡" },
		{ "Exif.Thumbnail.AntiAliasStrength", "缩略图.抗锯齿强度" },
		{ "Exif.Thumbnail.ApertureValue", "缩略图.光圈值" },
		{ "Exif.Thumbnail.Artist", "缩略图.艺术家" },
		{ "Exif.Thumbnail.AsShotICCProfile", "缩略图.拍摄时的ICC配置文件" },
		{ "Exif.Thumbnail.AsShotNeutral", "缩略图.拍摄时的中性点" },
		{ "Exif.Thumbnail.AsShotPreProfileMatrix", "缩略图.拍摄时的预配置矩阵" },
		{ "Exif.Thumbnail.AsShotProfileName", "缩略图.拍摄时的配置文件名称" },
		{ "Exif.Thumbnail.AsShotWhiteXY", "缩略图.拍摄时的白点XY坐标" },
		{ "Exif.Thumbnail.BaselineExposure", "缩略图.基准曝光" },
		{ "Exif.Thumbnail.BaselineNoise", "缩略图.基准噪点" },
		{ "Exif.Thumbnail.BaselineSharpness", "缩略图.基准锐度" },
		{ "Exif.Thumbnail.BatteryLevel", "缩略图.电池电量" },
		{ "Exif.Thumbnail.BayerGreenSplit", "缩略图.拜耳绿色分离" },
		{ "Exif.Thumbnail.BestQualityScale", "缩略图.最佳质量比例" },
		{ "Exif.Thumbnail.BitsPerSample", "缩略图.每个样本的位数" },
		{ "Exif.Thumbnail.BlackLevel", "缩略图.黑电平" },
		{ "Exif.Thumbnail.BlackLevelDeltaH", "缩略图.水平黑电平差值" },
		{ "Exif.Thumbnail.BlackLevelDeltaV", "缩略图.垂直黑电平差值" },
		{ "Exif.Thumbnail.BlackLevelRepeatDim", "缩略图.黑电平重复维度" },
		{ "Exif.Thumbnail.BrightnessValue", "缩略图.亮度值" },
		{ "Exif.Thumbnail.CalibrationIlluminant1", "缩略图.校准光源1" },
		{ "Exif.Thumbnail.CalibrationIlluminant2", "缩略图.校准光源2" },
		{ "Exif.Thumbnail.CameraCalibration1", "缩略图.相机校准1" },
		{ "Exif.Thumbnail.CameraCalibration2", "缩略图.相机校准2" },
		{ "Exif.Thumbnail.CameraCalibrationSignature", "缩略图.相机校准签名" },
		{ "Exif.Thumbnail.CameraSerialNumber", "缩略图.相机序列号" },
		{ "Exif.Thumbnail.CellLength", "缩略图.单元长度" },
		{ "Exif.Thumbnail.CellWidth", "缩略图.单元宽度" },
		{ "Exif.Thumbnail.CFALayout", "缩略图.CFA布局" },
		{ "Exif.Thumbnail.CFAPattern", "缩略图.CFA模式" },
		{ "Exif.Thumbnail.CFAPlaneColor", "缩略图.CFA平面颜色" },
		{ "Exif.Thumbnail.CFARepeatPatternDim", "缩略图.CFA重复模式维度" },
		{ "Exif.Thumbnail.ChromaBlurRadius", "缩略图.色度模糊半径" },
		{ "Exif.Thumbnail.ClipPath", "缩略图.裁剪路径" },
		{ "Exif.Thumbnail.ColorimetricReference", "缩略图.比色参考" },
		{ "Exif.Thumbnail.ColorMap", "缩略图.颜色映射" },
		{ "Exif.Thumbnail.ColorMatrix1", "缩略图.颜色矩阵1" },
		{ "Exif.Thumbnail.ColorMatrix2", "缩略图.颜色矩阵2" },
		{ "Exif.Thumbnail.CompressedBitsPerPixel", "缩略图.每像素压缩位数" },
		{ "Exif.Thumbnail.Compression", "缩略图.压缩" },
		{ "Exif.Thumbnail.Copyright", "缩略图.版权" },
		{ "Exif.Thumbnail.CurrentICCProfile", "缩略图.当前ICC配置文件" },
		{ "Exif.Thumbnail.CurrentPreProfileMatrix", "缩略图.当前预配置矩阵" },
		{ "Exif.Thumbnail.DateTime", "缩略图.日期时间" },
		{ "Exif.Thumbnail.DateTimeOriginal", "缩略图.原始日期时间" },
		{ "Exif.Thumbnail.DefaultCropOrigin", "缩略图.默认裁剪原点" },
		{ "Exif.Thumbnail.DefaultCropSize", "缩略图.默认裁剪大小" },
		{ "Exif.Thumbnail.DefaultScale", "缩略图.默认缩放" },
		{ "Exif.Thumbnail.Description", "缩略图.描述" },
		{ "Exif.Thumbnail.DNGBackwardVersion", "缩略图.DNG向后兼容版本" },
		{ "Exif.Thumbnail.DNGPrivateData", "缩略图.DNG私有数据" },
		{ "Exif.Thumbnail.DNGVersion", "缩略图.DNG版本" },
		{ "Exif.Thumbnail.DocumentName", "缩略图.文档名称" },
		{ "Exif.Thumbnail.DotRange", "缩略图.点范围" },
		{ "Exif.Thumbnail.ExifTag", "缩略图.Exif标签" },
		{ "Exif.Thumbnail.ExposureBiasValue", "缩略图.曝光补偿值" },
		{ "Exif.Thumbnail.ExposureIndex", "缩略图.曝光指数" },
		{ "Exif.Thumbnail.ExposureTime", "缩略图.曝光时间" },
		{ "Exif.Thumbnail.ExtraSamples", "缩略图.额外样本" },
		{ "Exif.Thumbnail.FillOrder", "缩略图.填充顺序" },
		{ "Exif.Thumbnail.Flash", "缩略图.闪光灯" },
		{ "Exif.Thumbnail.FlashEnergy", "缩略图.闪光灯能量" },
		{ "Exif.Thumbnail.FNumber", "缩略图.光圈值" },
		{ "Exif.Thumbnail.FocalLength", "缩略图.焦距" },
		{ "Exif.Thumbnail.FocalPlaneResolutionUnit", "缩略图.焦平面分辨率单位" },
		{ "Exif.Thumbnail.FocalPlaneXResolution", "缩略图.焦平面X分辨率" },
		{ "Exif.Thumbnail.FocalPlaneYResolution", "缩略图.焦平面Y分辨率" },
		{ "Exif.Thumbnail.ForwardMatrix1", "缩略图.前向矩阵1" },
		{ "Exif.Thumbnail.ForwardMatrix2", "缩略图.前向矩阵2" },
		{ "Exif.Thumbnail.GPSTag", "缩略图.GPS标签" },
		{ "Exif.Thumbnail.GrayResponseCurve", "缩略图.灰度响应曲线" },
		{ "Exif.Thumbnail.GrayResponseUnit", "缩略图.灰度响应单位" },
		{ "Exif.Thumbnail.HalftoneHints", "缩略图.半色调提示" },
		{ "Exif.Thumbnail.HostComputer", "缩略图.主机计算机" },
		{ "Exif.Thumbnail.ImageHistory", "缩略图.图像历史" },
		{ "Exif.Thumbnail.ImageID", "缩略图.图像ID" },
		{ "Exif.Thumbnail.ImageLength", "缩略图.图像长度" },
		{ "Exif.Thumbnail.ImageNumber", "缩略图.图像编号" },
		{ "Exif.Thumbnail.ImageResources", "缩略图.图像资源" },
		{ "Exif.Thumbnail.ImageWidth", "缩略图.图像宽度" },
		{ "Exif.Thumbnail.Indexed", "缩略图.索引" },
		{ "Exif.Thumbnail.InkNames", "缩略图.墨水名称" },
		{ "Exif.Thumbnail.InkSet", "缩略图.墨水集" },
		{ "Exif.Thumbnail.InterColorProfile", "缩略图.内部颜色配置文件" },
		{ "Exif.Thumbnail.Interlace", "缩略图.隔行扫描" },
		{ "Exif.Thumbnail.IPTC_NAA", "缩略图.IPTC/NAA" },
		{ "Exif.Thumbnail.ISOSpeedRatings", "缩略图.ISO速度等级" },
		{ "Exif.Thumbnail.JPEGACTables", "缩略图.JPEG AC表" },
		{ "Exif.Thumbnail.JPEGDCTables", "缩略图.JPEG DC表" },
		{ "Exif.Thumbnail.JPEGInterchangeFormat", "缩略图.JPEG交换格式" },
		{ "Exif.Thumbnail.JPEGInterchangeFormatLength", "缩略图.JPEG交换格式长度" },
		{ "Exif.Thumbnail.JPEGLosslessPredictors", "缩略图.JPEG无损预测器" },
		{ "Exif.Thumbnail.JPEGPointTransforms", "缩略图.JPEG点变换" },
		{ "Exif.Thumbnail.JPEGProc", "缩略图.JPEG处理" },
		{ "Exif.Thumbnail.JPEGQTables", "缩略图.JPEG量化表" },
		{ "Exif.Thumbnail.JPEGTables", "缩略图.JPEG表" },
		{ "Exif.Thumbnail.LensInfo", "缩略图.镜头信息" },
		{ "Exif.Thumbnail.LightSource", "缩略图.光源" },
		{ "Exif.Thumbnail.LinearizationTable", "缩略图.线性化表" },
		{ "Exif.Thumbnail.LinearResponseLimit", "缩略图.线性响应限制" },
		{ "Exif.Thumbnail.LocalizedCameraModel", "缩略图.本地化相机型号" },
		{ "Exif.Thumbnail.Make", "缩略图.制造商" },
		{ "Exif.Thumbnail.MakerNoteSafety", "缩略图.制造商备注安全" },
		{ "Exif.Thumbnail.MaskedAreas", "缩略图.遮蔽区域" },
		{ "Exif.Thumbnail.MaxApertureValue", "缩略图.最大光圈值" },
		{ "Exif.Thumbnail.MeteringMode", "缩略图.测光模式" },
		{ "Exif.Thumbnail.Model", "缩略图.型号" },
		{ "Exif.Thumbnail.NewSubfileType", "缩略图.新子文件类型" },
		{ "Exif.Thumbnail.Noise", "缩略图.噪点" },
		{ "Exif.Thumbnail.NoiseProfile", "缩略图.噪点配置文件" },
		{ "Exif.Thumbnail.NoiseReductionApplied", "缩略图.已应用降噪" },
		{ "Exif.Thumbnail.NumberOfInks", "缩略图.墨水数量" },
		{ "Exif.Thumbnail.OECF", "缩略图.光电转换函数" },
		{ "Exif.Thumbnail.OpcodeList1", "缩略图.操作码列表1" },
		{ "Exif.Thumbnail.OpcodeList2", "缩略图.操作码列表2" },
		{ "Exif.Thumbnail.OpcodeList3", "缩略图.操作码列表3" },
		{ "Exif.Thumbnail.OPIProxy", "缩略图.OPI代理" },
		{ "Exif.Thumbnail.Orientation", "缩略图.方向" },
		{ "Exif.Thumbnail.OriginalRawFileData", "缩略图.原始RAW文件数据" },
		{ "Exif.Thumbnail.OriginalRawFileDigest", "缩略图.原始RAW文件摘要" },
		{ "Exif.Thumbnail.OriginalRawFileName", "缩略图.原始RAW文件名" },
		{ "Exif.Thumbnail.PageNumber", "缩略图.页码" },
		{ "Exif.Thumbnail.PhotometricInterpretation", "缩略图.光度解释" },
		{ "Exif.Thumbnail.PlanarConfiguration", "缩略图.平面配置" },
		{ "Exif.Thumbnail.Predictor", "缩略图.预测器" },
		{ "Exif.Thumbnail.PreviewApplicationName", "缩略图.预览应用程序名称" },
		{ "Exif.Thumbnail.PreviewApplicationVersion", "缩略图.预览应用程序版本" },
		{ "Exif.Thumbnail.PreviewColorSpace", "缩略图.预览色彩空间" },
		{ "Exif.Thumbnail.PreviewDateTime", "缩略图.预览日期时间" },
		{ "Exif.Thumbnail.PreviewSettingsDigest", "缩略图.预览设置摘要" },
		{ "Exif.Thumbnail.PreviewSettingsName", "缩略图.预览设置名称" },
		{ "Exif.Thumbnail.PrimaryChromaticities", "缩略图.原色色度" },
		{ "Exif.Thumbnail.PrintImageMatching", "缩略图.打印图像匹配" },
		{ "Exif.Thumbnail.ProcessingSoftware", "缩略图.处理软件" },
		{ "Exif.Thumbnail.ProfileCalibrationSignature", "缩略图.配置文件校准签名" },
		{ "Exif.Thumbnail.ProfileCopyright", "缩略图.配置文件版权" },
		{ "Exif.Thumbnail.ProfileEmbedPolicy", "缩略图.配置文件嵌入策略" },
		{ "Exif.Thumbnail.ProfileHueSatMapData1", "缩略图.配置文件色相饱和度映射数据1" },
		{ "Exif.Thumbnail.ProfileHueSatMapData2", "缩略图.配置文件色相饱和度映射数据2" },
		{ "Exif.Thumbnail.ProfileHueSatMapDims", "缩略图.配置文件色相饱和度映射维度" },
		{ "Exif.Thumbnail.ProfileLookTableData", "缩略图.配置文件查找表数据" },
		{ "Exif.Thumbnail.ProfileLookTableDims", "缩略图.配置文件查找表维度" },
		{ "Exif.Thumbnail.ProfileName", "缩略图.配置文件名称" },
		{ "Exif.Thumbnail.ProfileToneCurve", "缩略图.配置文件色调曲线" },
		{ "Exif.Thumbnail.Rating", "缩略图.评分" },
		{ "Exif.Thumbnail.RatingPercent", "缩略图.评分百分比" },
		{ "Exif.Thumbnail.RawDataUniqueID", "缩略图.原始数据唯一ID" },
		{ "Exif.Thumbnail.RawImageDigest", "缩略图.原始图像摘要" },
		{ "Exif.Thumbnail.ReductionMatrix1", "缩略图.缩减矩阵1" },
		{ "Exif.Thumbnail.ReductionMatrix2", "缩略图.缩减矩阵2" },
		{ "Exif.Thumbnail.ReferenceBlackWhite", "缩略图.参考黑白点" },
		{ "Exif.Thumbnail.ResolutionUnit", "缩略图.分辨率单位" },
		{ "Exif.Thumbnail.RowInterleaveFactor", "缩略图.行交错因子" },
		{ "Exif.Thumbnail.RowsPerStrip", "缩略图.每条带行数" },
		{ "Exif.Thumbnail.SampleFormat", "缩略图.样本格式" },
		{ "Exif.Thumbnail.SamplesPerPixel", "缩略图.每像素样本数" },
		{ "Exif.Thumbnail.SecurityClassification", "缩略图.安全分类" },
		{ "Exif.Thumbnail.SelfTimerMode", "缩略图.自拍模式" },
		{ "Exif.Thumbnail.SensingMethod", "缩略图.感应方法" },
		{ "Exif.Thumbnail.ShadowScale", "缩略图.阴影比例" },
		{ "Exif.Thumbnail.ShutterSpeedValue", "缩略图.快门速度值" },
		{ "Exif.Thumbnail.SMaxSampleValue", "缩略图.最大样本值" },
		{ "Exif.Thumbnail.SMinSampleValue", "缩略图.最小样本值" },
		{ "Exif.Thumbnail.Software", "缩略图.软件" },
		{ "Exif.Thumbnail.SpatialFrequencyResponse", "缩略图.空间频率响应" },
		{ "Exif.Thumbnail.SpectralSensitivity", "缩略图.光谱灵敏度" },
		{ "Exif.Thumbnail.StripByteCounts", "缩略图.条带字节计数" },
		{ "Exif.Thumbnail.StripOffsets", "缩略图.条带偏移量" },
		{ "Exif.Thumbnail.SubfileType", "缩略图.子文件类型" },
		{ "Exif.Thumbnail.SubIFD", "缩略图.子IFD" },
		{ "Exif.Thumbnail.SubjectDistance", "缩略图.主体距离" },
		{ "Exif.Thumbnail.SubjectLocation", "缩略图.主体位置" },
		{ "Exif.Thumbnail.SubTileBlockSize", "缩略图.子块大小" },
		{ "Exif.Thumbnail.T4Options", "缩略图.T4选项" },
		{ "Exif.Thumbnail.T6Options", "缩略图.T6选项" },
		{ "Exif.Thumbnail.TargetPrinter", "缩略图.目标打印机" },
		{ "Exif.Thumbnail.Threshholding", "缩略图.阈值处理" },
		{ "Exif.Thumbnail.TIFFEPStandardID", "缩略图.TIFFEP标准ID" },
		{ "Exif.Thumbnail.TileByteCounts", "缩略图.块字节数" },
		{ "Exif.Thumbnail.TileLength", "缩略图.块长度" },
		{ "Exif.Thumbnail.TileOffsets", "缩略图.块偏移量" },
		{ "Exif.Thumbnail.TileWidth", "缩略图.块宽度" },
		{ "Exif.Thumbnail.TimeZoneOffset", "缩略图.时区偏移" },
		{ "Exif.Thumbnail.TransferFunction", "缩略图.传输函数" },
		{ "Exif.Thumbnail.TransferRange", "缩略图.传输范围" },
		{ "Exif.Thumbnail.UniqueCameraModel", "缩略图.唯一相机型号" },
		{ "Exif.Thumbnail.WhiteLevel", "缩略图.白色级别" },
		{ "Exif.Thumbnail.WhitePoint", "缩略图.白点" },
		{ "Exif.Thumbnail.XClipPathUnits", "缩略图.X剪裁路径单位" },
		{ "Exif.Thumbnail.XMLPacket", "缩略图.XML数据包" },
		{ "Exif.Thumbnail.XPAuthor", "缩略图.XP作者" },
		{ "Exif.Thumbnail.XPComment", "缩略图.XP评论" },
		{ "Exif.Thumbnail.XPKeywords", "缩略图.XP关键词" },
		{ "Exif.Thumbnail.XPSubject", "缩略图.XP主题" },
		{ "Exif.Thumbnail.XPTitle", "缩略图.XP标题" },
		{ "Exif.Thumbnail.XResolution", "缩略图.X分辨率" },
		{ "Exif.Thumbnail.YCbCrCoefficients", "缩略图.YCbCr系数" },
		{ "Exif.Thumbnail.YCbCrPositioning", "缩略图.YCbCr定位" },
		{ "Exif.Thumbnail.YCbCrSubSampling", "缩略图.YCbCr子采样" },
		{ "Exif.Thumbnail.YClipPathUnits", "缩略图.Y剪裁路径单位" },
		{ "Exif.Thumbnail.YResolution", "缩略图.Y分辨率" },

		{ "Exif.SubImage1.ProcessingSoftware", "子图1.处理软件" },
		{ "Exif.SubImage1.NewSubfileType", "子图1.新子文件类型" },
		{ "Exif.SubImage1.SubfileType", "子图1.子文件类型" },
		{ "Exif.SubImage1.ImageWidth", "子图1.图像宽度" },
		{ "Exif.SubImage1.ImageLength", "子图1.图像长度" },
		{ "Exif.SubImage1.BitsPerSample", "子图1.每个样本的位数" },
		{ "Exif.SubImage1.Compression", "子图1.压缩" },
		{ "Exif.SubImage1.PhotometricInterpretation", "子图1.光度解释" },
		{ "Exif.SubImage1.Thresholding", "子图1.阈值处理" },
		{ "Exif.SubImage1.CellWidth", "子图1.单元格宽度" },
		{ "Exif.SubImage1.CellLength", "子图1.单元格长度" },
		{ "Exif.SubImage1.FillOrder", "子图1.填充顺序" },
		{ "Exif.SubImage1.DocumentName", "子图1.文档名称" },
		{ "Exif.SubImage1.ImageDescription", "子图1.图像描述" },
		{ "Exif.SubImage1.Make", "子图1.制造商" },
		{ "Exif.SubImage1.Model", "子图1.型号" },
		{ "Exif.SubImage1.StripOffsets", "子图1.条带偏移量" },
		{ "Exif.SubImage1.Orientation", "子图1.方向" },
		{ "Exif.SubImage1.SamplesPerPixel", "子图1.每像素样本数" },
		{ "Exif.SubImage1.RowsPerStrip", "子图1.每条带行数" },
		{ "Exif.SubImage1.StripByteCounts", "子图1.条带字节数" },
		{ "Exif.SubImage1.XResolution", "子图1.X分辨率" },
		{ "Exif.SubImage1.YResolution", "子图1.Y分辨率" },
		{ "Exif.SubImage1.PlanarConfiguration", "子图1.平面配置" },
		{ "Exif.SubImage1.PageName", "子图1.页面名称" },
		{ "Exif.SubImage1.XPosition", "子图1.X位置" },
		{ "Exif.SubImage1.YPosition", "子图1.Y位置" },
		{ "Exif.SubImage1.GrayResponseUnit", "子图1.灰度响应单位" },
		{ "Exif.SubImage1.GrayResponseCurve", "子图1.灰度响应曲线" },
		{ "Exif.SubImage1.T4Options", "子图1.T4选项" },
		{ "Exif.SubImage1.T6Options", "子图1.T6选项" },
		{ "Exif.SubImage1.ResolutionUnit", "子图1.分辨率单位" },
		{ "Exif.SubImage1.PageNumber", "子图1.页码" },
		{ "Exif.SubImage1.TransferFunction", "子图1.传输函数" },
		{ "Exif.SubImage1.Software", "子图1.软件" },
		{ "Exif.SubImage1.DateTime", "子图1.日期时间" },
		{ "Exif.SubImage1.Artist", "子图1.艺术家" },
		{ "Exif.SubImage1.HostComputer", "子图1.主机计算机" },
		{ "Exif.SubImage1.Predictor", "子图1.预测器" },
		{ "Exif.SubImage1.WhitePoint", "子图1.白点" },
		{ "Exif.SubImage1.PrimaryChromaticities", "子图1.原色色度" },
		{ "Exif.SubImage1.ColorMap", "子图1.颜色映射" },
		{ "Exif.SubImage1.HalftoneHints", "子图1.半色调提示" },
		{ "Exif.SubImage1.TileWidth", "子图1.瓦片宽度" },
		{ "Exif.SubImage1.TileLength", "子图1.瓦片长度" },
		{ "Exif.SubImage1.TileOffsets", "子图1.瓦片偏移量" },
		{ "Exif.SubImage1.TileByteCounts", "子图1.瓦片字节数" },
		{ "Exif.SubImage1.SubIFDs", "子图1.子IFDs" },
		{ "Exif.SubImage1.InkSet", "子图1.墨水集" },
		{ "Exif.SubImage1.InkNames", "子图1.墨水名称" },
		{ "Exif.SubImage1.NumberOfInks", "子图1.墨水数量" },
		{ "Exif.SubImage1.DotRange", "子图1.点范围" },
		{ "Exif.SubImage1.TargetPrinter", "子图1.目标打印机" },
		{ "Exif.SubImage1.ExtraSamples", "子图1.额外样本" },
		{ "Exif.SubImage1.SampleFormat", "子图1.样本格式" },
		{ "Exif.SubImage1.SMinSampleValue", "子图1.最小样本值" },
		{ "Exif.SubImage1.SMaxSampleValue", "子图1.最大样本值" },
		{ "Exif.SubImage1.TransferRange", "子图1.传输范围" },
		{ "Exif.SubImage1.ClipPath", "子图1.裁剪路径" },
		{ "Exif.SubImage1.XClipPathUnits", "子图1.X裁剪路径单位" },
		{ "Exif.SubImage1.YClipPathUnits", "子图1.Y裁剪路径单位" },
		{ "Exif.SubImage1.Indexed", "子图1.索引" },
		{ "Exif.SubImage1.JPEGTables", "子图1.JPEG表" },
		{ "Exif.SubImage1.OPIProxy", "子图1.OPI代理" },
		{ "Exif.SubImage1.JPEGProc", "子图1.JPEG处理" },
		{ "Exif.SubImage1.JPEGInterchangeFormat", "子图1.JPEG交换格式" },
		{ "Exif.SubImage1.JPEGInterchangeFormatLength", "子图1.JPEG交换格式长度" },
		{ "Exif.SubImage1.JPEGRestartInterval", "子图1.JPEG重启间隔" },
		{ "Exif.SubImage1.JPEGLosslessPredictors", "子图1.JPEG无损预测器" },
		{ "Exif.SubImage1.JPEGPointTransforms", "子图1.JPEG点变换" },
		{ "Exif.SubImage1.JPEGQTables", "子图1.JPEG量化表" },
		{ "Exif.SubImage1.JPEGDCTables", "子图1.JPEG直流表" },
		{ "Exif.SubImage1.JPEGACTables", "子图1.JPEG交流表" },
		{ "Exif.SubImage1.YCbCrCoefficients", "子图1.YCbCr系数" },
		{ "Exif.SubImage1.YCbCrSubSampling", "子图1.YCbCr子采样" },
		{ "Exif.SubImage1.YCbCrPositioning", "子图1.YCbCr定位" },
		{ "Exif.SubImage1.ReferenceBlackWhite", "子图1.参考黑白点" },
		{ "Exif.SubImage1.XMLPacket", "子图1.XML数据包" },
		{ "Exif.SubImage1.Rating", "子图1.评分" },
		{ "Exif.SubImage1.RatingPercent", "子图1.评分百分比" },
		{ "Exif.SubImage1.VignettingCorrParams", "子图1.暗角校正参数" },
		{ "Exif.SubImage1.ChromaticAberrationCorrParams", "子图1.色差校正参数" },
		{ "Exif.SubImage1.DistortionCorrParams", "子图1.畸变校正参数" },
		{ "Exif.SubImage1.ImageID", "子图1.图像ID" },
		{ "Exif.SubImage1.CFARepeatPatternDim", "子图1.CFA重复模式尺寸" },
		{ "Exif.SubImage1.CFAPattern", "子图1.CFA模式" },
		{ "Exif.SubImage1.BatteryLevel", "子图1.电池电量" },
		{ "Exif.SubImage1.Copyright", "子图1.版权" },
		{ "Exif.SubImage1.ExposureTime", "子图1.曝光时间" },
		{ "Exif.SubImage1.FNumber", "子图1.光圈值" },
		{ "Exif.SubImage1.IPTCNAA", "子图1.IPTC/NAA" },
		{ "Exif.SubImage1.ImageResources", "子图1.图像资源" },
		{ "Exif.SubImage1.ExifTag", "子图1.Exif标签" },
		{ "Exif.SubImage1.InterColorProfile", "子图1.内部颜色配置文件" },
		{ "Exif.SubImage1.ExposureProgram", "子图1.曝光程序" },
		{ "Exif.SubImage1.SpectralSensitivity", "子图1.光谱敏感度" },
		{ "Exif.SubImage1.GPSTag", "子图1.GPS标签" },
		{ "Exif.SubImage1.ISOSpeedRatings", "子图1.ISO速度等级" },
		{ "Exif.SubImage1.OECF", "子图1.光电转换函数" },
		{ "Exif.SubImage1.Interlace", "子图1.隔行扫描" },
		{ "Exif.SubImage1.TimeZoneOffset", "子图1.时区偏移" },
		{ "Exif.SubImage1.SelfTimerMode", "子图1.自拍模式" },
		{ "Exif.SubImage1.DateTimeOriginal", "子图1.原始日期时间" },
		{ "Exif.SubImage1.CompressedBitsPerPixel", "子图1.每像素压缩位数" },
		{ "Exif.SubImage1.ShutterSpeedValue", "子图1.快门速度值" },
		{ "Exif.SubImage1.ApertureValue", "子图1.光圈值" },
		{ "Exif.SubImage1.BrightnessValue", "子图1.亮度值" },
		{ "Exif.SubImage1.ExposureBiasValue", "子图1.曝光补偿值" },
		{ "Exif.SubImage1.MaxApertureValue", "子图1.最大光圈值" },
		{ "Exif.SubImage1.SubjectDistance", "子图1.主体距离" },
		{ "Exif.SubImage1.MeteringMode", "子图1.测光模式" },
		{ "Exif.SubImage1.LightSource", "子图1.光源" },
		{ "Exif.SubImage1.Flash", "子图1.闪光灯" },
		{ "Exif.SubImage1.FocalLength", "子图1.焦距" },
		{ "Exif.SubImage1.FlashEnergy", "子图1.闪光灯能量" },
		{ "Exif.SubImage1.SpatialFrequencyResponse", "子图1.空间频率响应" },
		{ "Exif.SubImage1.Noise", "子图1.噪声" },
		{ "Exif.SubImage1.FocalPlaneXResolution", "子图1.焦平面X分辨率" },
		{ "Exif.SubImage1.FocalPlaneYResolution", "子图1.焦平面Y分辨率" },
		{ "Exif.SubImage1.FocalPlaneResolutionUnit", "子图1.焦平面分辨率单位" },
		{ "Exif.SubImage1.ImageNumber", "子图1.图像编号" },
		{ "Exif.SubImage1.SecurityClassification", "子图1.安全分类" },
		{ "Exif.SubImage1.ImageHistory", "子图1.图像历史" },
		{ "Exif.SubImage1.SubjectLocation", "子图1.主体位置" },
		{ "Exif.SubImage1.ExposureIndex", "子图1.曝光指数" },
		{ "Exif.SubImage1.TIFFEPStandardID", "子图1.TIFF/EP标准ID" },
		{ "Exif.SubImage1.SensingMethod", "子图1.感应方法" },
		{ "Exif.SubImage1.XPTitle", "子图1.XP标题" },
		{ "Exif.SubImage1.XPComment", "子图1.XP注释" },
		{ "Exif.SubImage1.XPAuthor", "子图1.XP作者" },
		{ "Exif.SubImage1.XPKeywords", "子图1.XP关键词" },
		{ "Exif.SubImage1.XPSubject", "子图1.XP主题" },
		{ "Exif.SubImage1.PrintImageMatching", "子图1.打印图像匹配" },
		{ "Exif.SubImage1.DNGVersion", "子图1.DNG版本" },
		{ "Exif.SubImage1.DNGBackwardVersion", "子图1.DNG向后兼容版本" },
		{ "Exif.SubImage1.UniqueCameraModel", "子图1.唯一相机型号" },
		{ "Exif.SubImage1.LocalizedCameraModel", "子图1.本地化相机型号" },
		{ "Exif.SubImage1.CFAPlaneColor", "子图1.CFA平面颜色" },
		{ "Exif.SubImage1.CFALayout", "子图1.CFA布局" },
		{ "Exif.SubImage1.LinearizationTable", "子图1.线性化表" },
		{ "Exif.SubImage1.BlackLevelRepeatDim", "子图1.黑电平重复尺寸" },
		{ "Exif.SubImage1.BlackLevel", "子图1.黑电平" },
		{ "Exif.SubImage1.BlackLevelDeltaH", "子图1.水平黑电平增量" },
		{ "Exif.SubImage1.BlackLevelDeltaV", "子图1.垂直黑电平增量" },
		{ "Exif.SubImage1.WhiteLevel", "子图1.白电平" },
		{ "Exif.SubImage1.DefaultScale", "子图1.默认比例" },
		{ "Exif.SubImage1.DefaultCropOrigin", "子图1.默认裁剪原点" },
		{ "Exif.SubImage1.DefaultCropSize", "子图1.默认裁剪尺寸" },
		{ "Exif.SubImage1.ColorMatrix1", "子图1.颜色矩阵1" },
		{ "Exif.SubImage1.ColorMatrix2", "子图1.颜色矩阵2" },
		{ "Exif.SubImage1.CameraCalibration1", "子图1.相机校准1" },
		{ "Exif.SubImage1.CameraCalibration2", "子图1.相机校准2" },
		{ "Exif.SubImage1.ReductionMatrix1", "子图1.缩减矩阵1" },
		{ "Exif.SubImage1.ReductionMatrix2", "子图1.缩减矩阵2" },
		{ "Exif.SubImage1.AnalogBalance", "子图1.模拟平衡" },
		{ "Exif.SubImage1.AsShotNeutral", "子图1.拍摄时的中性点" },
		{ "Exif.SubImage1.AsShotWhiteXY", "子图1.拍摄时的白点XY坐标" },
		{ "Exif.SubImage1.BaselineExposure", "子图1.基准曝光" },
		{ "Exif.SubImage1.BaselineNoise", "子图1.基准噪声" },
		{ "Exif.SubImage1.BaselineSharpness", "子图1.基准锐度" },
		{ "Exif.SubImage1.BayerGreenSplit", "子图1.拜耳绿色分离" },
		{ "Exif.SubImage1.LinearResponseLimit", "子图1.线性响应限制" },
		{ "Exif.SubImage1.CameraSerialNumber", "子图1.相机序列号" },
		{ "Exif.SubImage1.LensInfo", "子图1.镜头信息" },
		{ "Exif.SubImage1.ChromaBlurRadius", "子图1.色度模糊半径" },
		{ "Exif.SubImage1.AntiAliasStrength", "子图1.抗锯齿强度" },
		{ "Exif.SubImage1.ShadowScale", "子图1.阴影比例" },
		{ "Exif.SubImage1.DNGPrivateData", "子图1.DNG私有数据" },
		{ "Exif.SubImage1.CalibrationIlluminant1", "子图1.校准光源1" },
		{ "Exif.SubImage1.CalibrationIlluminant2", "子图1.校准光源2" },
		{ "Exif.SubImage1.BestQualityScale", "子图1.最佳质量比例" },
		{ "Exif.SubImage1.RawDataUniqueID", "子图1.原始数据唯一ID" },
		{ "Exif.SubImage1.OriginalRawFileName", "子图1.原始RAW文件名" },
		{ "Exif.SubImage1.OriginalRawFileData", "子图1.原始RAW文件数据" },
		{ "Exif.SubImage1.ActiveArea", "子图1.有效区域" },
		{ "Exif.SubImage1.MaskedAreas", "子图1.遮罩区域" },
		{ "Exif.SubImage1.AsShotICCProfile", "子图1.拍摄时的ICC配置文件" },
		{ "Exif.SubImage1.AsShotPreProfileMatrix", "子图1.拍摄时的预配置文件矩阵" },
		{ "Exif.SubImage1.CurrentICCProfile", "子图1.当前ICC配置文件" },
		{ "Exif.SubImage1.CurrentPreProfileMatrix", "子图1.当前预配置文件矩阵" },
		{ "Exif.SubImage1.ColorimetricReference", "子图1.比色参考" },
		{ "Exif.SubImage1.CameraCalibrationSignature", "子图1.相机校准签名" },
		{ "Exif.SubImage1.ProfileCalibrationSignature", "子图1.配置文件校准签名" },
		{ "Exif.SubImage1.ExtraCameraProfiles", "子图1.额外相机配置文件" },
		{ "Exif.SubImage1.AsShotProfileName", "子图1.拍摄时配置文件名称" },
		{ "Exif.SubImage1.NoiseReductionApplied", "子图1.已应用的降噪" },
		{ "Exif.SubImage1.ProfileName", "子图1.配置文件名称" },
		{ "Exif.SubImage1.ProfileHueSatMapDims", "子图1.配置文件色相饱和度映射维度" },
		{ "Exif.SubImage1.ProfileHueSatMapData1", "子图1.配置文件色相饱和度映射数据1" },
		{ "Exif.SubImage1.ProfileHueSatMapData2", "子图1.配置文件色相饱和度映射数据2" },
		{ "Exif.SubImage1.ProfileToneCurve", "子图1.配置文件色调曲线" },
		{ "Exif.SubImage1.ProfileEmbedPolicy", "子图1.配置文件嵌入策略" },
		{ "Exif.SubImage1.ProfileCopyright", "子图1.配置文件版权" },
		{ "Exif.SubImage1.ForwardMatrix1", "子图1.前向矩阵1" },
		{ "Exif.SubImage1.ForwardMatrix2", "子图1.前向矩阵2" },
		{ "Exif.SubImage1.PreviewApplicationName", "子图1.预览应用程序名称" },
		{ "Exif.SubImage1.PreviewApplicationVersion", "子图1.预览应用程序版本" },
		{ "Exif.SubImage1.PreviewSettingsName", "子图1.预览设置名称" },
		{ "Exif.SubImage1.PreviewSettingsDigest", "子图1.预览设置摘要" },
		{ "Exif.SubImage1.PreviewColorSpace", "子图1.预览色彩空间" },
		{ "Exif.SubImage1.PreviewDateTime", "子图1.预览日期时间" },
		{ "Exif.SubImage1.RawImageDigest", "子图1.原始图像摘要" },
		{ "Exif.SubImage1.OriginalRawFileDigest", "子图1.原始RAW文件摘要" },
		{ "Exif.SubImage1.SubTileBlockSize", "子图1.子块大小" },
		{ "Exif.SubImage1.RowInterleaveFactor", "子图1.行交错因子" },
		{ "Exif.SubImage1.ProfileLookTableDims", "子图1.配置文件查找表维度" },
		{ "Exif.SubImage1.ProfileLookTableData", "子图1.配置文件查找表数据" },
		{ "Exif.SubImage1.OpcodeList1", "子图1.操作码列表1" },
		{ "Exif.SubImage1.OpcodeList2", "子图1.操作码列表2" },
		{ "Exif.SubImage1.OpcodeList3", "子图1.操作码列表3" },
		{ "Exif.SubImage1.NoiseProfile", "子图1.噪声配置文件" },
		{ "Exif.SubImage1.TimeCodes", "子图1.时间码" },
		{ "Exif.SubImage1.FrameRate", "子图1.帧率" },
		{ "Exif.SubImage1.TStop", "子图1.T档" },
		{ "Exif.SubImage1.ReelName", "子图1.胶片名称" },
		{ "Exif.SubImage1.CameraLabel", "子图1.相机标签" },
		{ "Exif.SubImage1.OriginalDefaultFinalSize", "子图1.原始默认最终尺寸" },
		{ "Exif.SubImage1.OriginalBestQualityFinalSize", "子图1.原始最佳质量最终尺寸" },
		{ "Exif.SubImage1.OriginalDefaultCropSize", "子图1.原始默认裁剪尺寸" },
		{ "Exif.SubImage1.ProfileHueSatMapEncoding", "子图1.配置文件色相饱和度映射编码" },
		{ "Exif.SubImage1.ProfileLookTableEncoding", "子图1.配置文件查找表编码" },
		{ "Exif.SubImage1.BaselineExposureOffset", "子图1.基准曝光偏移" },
		{ "Exif.SubImage1.DefaultBlackRender", "子图1.默认黑色渲染" },
		{ "Exif.SubImage1.NewRawImageDigest", "子图1.新原始图像摘要" },
		{ "Exif.SubImage1.RawToPreviewGain", "子图1.原始到预览增益" },
		{ "Exif.SubImage1.DefaultUserCrop", "子图1.默认用户裁剪" },
		{ "Exif.SubImage1.DepthFormat", "子图1.深度格式" },
		{ "Exif.SubImage1.DepthNear", "子图1.近深度" },
		{ "Exif.SubImage1.DepthFar", "子图1.远深度" },
		{ "Exif.SubImage1.DepthUnits", "子图1.深度单位" },
		{ "Exif.SubImage1.DepthMeasureType", "子图1.深度测量类型" },
		{ "Exif.SubImage1.EnhanceParams", "子图1.增强参数" },
		{ "Exif.SubImage1.ProfileGainTableMap", "子图1.配置文件增益表映射" },
		{ "Exif.SubImage1.SemanticName", "子图1.语义名称" },
		{ "Exif.SubImage1.SemanticInstanceID", "子图1.语义实例ID" },
		{ "Exif.SubImage1.CalibrationIlluminant3", "子图1.校准光源3" },
		{ "Exif.SubImage1.CameraCalibration3", "子图1.相机校准3" },
		{ "Exif.SubImage1.ColorMatrix3", "子图1.颜色矩阵3" },
		{ "Exif.SubImage1.ForwardMatrix3", "子图1.前向矩阵3" },
		{ "Exif.SubImage1.IlluminantData1", "子图1.光源数据1" },
		{ "Exif.SubImage1.IlluminantData2", "子图1.光源数据2" },
		{ "Exif.SubImage1.IlluminantData3", "子图1.光源数据3" },
		{ "Exif.SubImage1.MaskSubArea", "子图1.遮罩子区域" },
		{ "Exif.SubImage1.ProfileHueSatMapData3", "子图1.配置文件色相饱和度映射数据3" },
		{ "Exif.SubImage1.ReductionMatrix3", "子图1.缩减矩阵3" },
		{ "Exif.SubImage1.RGBTables", "子图1.RGB表" },
		{ "Exif.SubImage1.ProfileGainTableMap2", "子图1.配置文件增益表映射2" },
		{ "Exif.SubImage1.ColumnInterleaveFactor", "子图1.列交错因子" },
		{ "Exif.SubImage1.ImageSequenceInfo", "子图1.图像序列信息" },
		{ "Exif.SubImage1.ImageStats", "子图1.图像统计" },
		{ "Exif.SubImage1.ProfileDynamicRange", "子图1.配置文件动态范围" },
		{ "Exif.SubImage1.ProfileGroupName", "子图1.配置文件组名称" },
		{ "Exif.SubImage1.JXLDistance", "子图1.JXL距离" },
		{ "Exif.SubImage1.JXLEffort", "子图1.JXL努力程度" },
		{ "Exif.SubImage1.JXLDecodeSpeed", "子图1.JXL解码速度" },

		{ "Exif.SubImage2.ProcessingSoftware", "子图2.处理软件" },
		{ "Exif.SubImage2.NewSubfileType", "子图2.新子文件类型" },
		{ "Exif.SubImage2.SubfileType", "子图2.子文件类型" },
		{ "Exif.SubImage2.ImageWidth", "子图2.图像宽度" },
		{ "Exif.SubImage2.ImageLength", "子图2.图像长度" },
		{ "Exif.SubImage2.BitsPerSample", "子图2.每个样本的位数" },
		{ "Exif.SubImage2.Compression", "子图2.压缩" },
		{ "Exif.SubImage2.PhotometricInterpretation", "子图2.光度解释" },
		{ "Exif.SubImage2.Thresholding", "子图2.阈值处理" },
		{ "Exif.SubImage2.CellWidth", "子图2.单元格宽度" },
		{ "Exif.SubImage2.CellLength", "子图2.单元格长度" },
		{ "Exif.SubImage2.FillOrder", "子图2.填充顺序" },
		{ "Exif.SubImage2.DocumentName", "子图2.文档名称" },
		{ "Exif.SubImage2.ImageDescription", "子图2.图像描述" },
		{ "Exif.SubImage2.Make", "子图2.制造商" },
		{ "Exif.SubImage2.Model", "子图2.型号" },
		{ "Exif.SubImage2.StripOffsets", "子图2.条带偏移量" },
		{ "Exif.SubImage2.Orientation", "子图2.方向" },
		{ "Exif.SubImage2.SamplesPerPixel", "子图2.每像素样本数" },
		{ "Exif.SubImage2.RowsPerStrip", "子图2.每条带行数" },
		{ "Exif.SubImage2.StripByteCounts", "子图2.条带字节数" },
		{ "Exif.SubImage2.XResolution", "子图2.X分辨率" },
		{ "Exif.SubImage2.YResolution", "子图2.Y分辨率" },
		{ "Exif.SubImage2.PlanarConfiguration", "子图2.平面配置" },
		{ "Exif.SubImage2.PageName", "子图2.页面名称" },
		{ "Exif.SubImage2.XPosition", "子图2.X位置" },
		{ "Exif.SubImage2.YPosition", "子图2.Y位置" },
		{ "Exif.SubImage2.GrayResponseUnit", "子图2.灰度响应单位" },
		{ "Exif.SubImage2.GrayResponseCurve", "子图2.灰度响应曲线" },
		{ "Exif.SubImage2.T4Options", "子图2.T4选项" },
		{ "Exif.SubImage2.T6Options", "子图2.T6选项" },
		{ "Exif.SubImage2.ResolutionUnit", "子图2.分辨率单位" },
		{ "Exif.SubImage2.PageNumber", "子图2.页码" },
		{ "Exif.SubImage2.TransferFunction", "子图2.传输函数" },
		{ "Exif.SubImage2.Software", "子图2.软件" },
		{ "Exif.SubImage2.DateTime", "子图2.日期时间" },
		{ "Exif.SubImage2.Artist", "子图2.艺术家" },
		{ "Exif.SubImage2.HostComputer", "子图2.主机计算机" },
		{ "Exif.SubImage2.Predictor", "子图2.预测器" },
		{ "Exif.SubImage2.WhitePoint", "子图2.白点" },
		{ "Exif.SubImage2.PrimaryChromaticities", "子图2.原色色度" },
		{ "Exif.SubImage2.ColorMap", "子图2.颜色映射" },
		{ "Exif.SubImage2.HalftoneHints", "子图2.半色调提示" },
		{ "Exif.SubImage2.TileWidth", "子图2.瓦片宽度" },
		{ "Exif.SubImage2.TileLength", "子图2.瓦片长度" },
		{ "Exif.SubImage2.TileOffsets", "子图2.瓦片偏移量" },
		{ "Exif.SubImage2.TileByteCounts", "子图2.瓦片字节数" },
		{ "Exif.SubImage2.SubIFDs", "子图2.子IFDs" },
		{ "Exif.SubImage2.InkSet", "子图2.墨水集" },
		{ "Exif.SubImage2.InkNames", "子图2.墨水名称" },
		{ "Exif.SubImage2.NumberOfInks", "子图2.墨水数量" },
		{ "Exif.SubImage2.DotRange", "子图2.点范围" },
		{ "Exif.SubImage2.TargetPrinter", "子图2.目标打印机" },
		{ "Exif.SubImage2.ExtraSamples", "子图2.额外样本" },
		{ "Exif.SubImage2.SampleFormat", "子图2.样本格式" },
		{ "Exif.SubImage2.SMinSampleValue", "子图2.最小样本值" },
		{ "Exif.SubImage2.SMaxSampleValue", "子图2.最大样本值" },
		{ "Exif.SubImage2.TransferRange", "子图2.传输范围" },
		{ "Exif.SubImage2.ClipPath", "子图2.裁剪路径" },
		{ "Exif.SubImage2.XClipPathUnits", "子图2.X裁剪路径单位" },
		{ "Exif.SubImage2.YClipPathUnits", "子图2.Y裁剪路径单位" },
		{ "Exif.SubImage2.Indexed", "子图2.索引" },
		{ "Exif.SubImage2.JPEGTables", "子图2.JPEG表" },
		{ "Exif.SubImage2.OPIProxy", "子图2.OPI代理" },
		{ "Exif.SubImage2.JPEGProc", "子图2.JPEG处理" },
		{ "Exif.SubImage2.JPEGInterchangeFormat", "子图2.JPEG交换格式" },
		{ "Exif.SubImage2.JPEGInterchangeFormatLength", "子图2.JPEG交换格式长度" },
		{ "Exif.SubImage2.JPEGRestartInterval", "子图2.JPEG重启间隔" },
		{ "Exif.SubImage2.JPEGLosslessPredictors", "子图2.JPEG无损预测器" },
		{ "Exif.SubImage2.JPEGPointTransforms", "子图2.JPEG点变换" },
		{ "Exif.SubImage2.JPEGQTables", "子图2.JPEG量化表" },
		{ "Exif.SubImage2.JPEGDCTables", "子图2.JPEG直流表" },
		{ "Exif.SubImage2.JPEGACTables", "子图2.JPEG交流表" },
		{ "Exif.SubImage2.YCbCrCoefficients", "子图2.YCbCr系数" },
		{ "Exif.SubImage2.YCbCrSubSampling", "子图2.YCbCr子采样" },
		{ "Exif.SubImage2.YCbCrPositioning", "子图2.YCbCr定位" },
		{ "Exif.SubImage2.ReferenceBlackWhite", "子图2.参考黑白点" },
		{ "Exif.SubImage2.XMLPacket", "子图2.XML数据包" },
		{ "Exif.SubImage2.Rating", "子图2.评分" },
		{ "Exif.SubImage2.RatingPercent", "子图2.评分百分比" },
		{ "Exif.SubImage2.VignettingCorrParams", "子图2.暗角校正参数" },
		{ "Exif.SubImage2.ChromaticAberrationCorrParams", "子图2.色差校正参数" },
		{ "Exif.SubImage2.DistortionCorrParams", "子图2.畸变校正参数" },
		{ "Exif.SubImage2.ImageID", "子图2.图像ID" },
		{ "Exif.SubImage2.CFARepeatPatternDim", "子图2.CFA重复模式尺寸" },
		{ "Exif.SubImage2.CFAPattern", "子图2.CFA模式" },
		{ "Exif.SubImage2.BatteryLevel", "子图2.电池电量" },
		{ "Exif.SubImage2.Copyright", "子图2.版权" },
		{ "Exif.SubImage2.ExposureTime", "子图2.曝光时间" },
		{ "Exif.SubImage2.FNumber", "子图2.光圈值" },
		{ "Exif.SubImage2.IPTCNAA", "子图2.IPTC/NAA" },
		{ "Exif.SubImage2.ImageResources", "子图2.图像资源" },
		{ "Exif.SubImage2.ExifTag", "子图2.Exif标签" },
		{ "Exif.SubImage2.InterColorProfile", "子图2.内部颜色配置文件" },
		{ "Exif.SubImage2.ExposureProgram", "子图2.曝光程序" },
		{ "Exif.SubImage2.SpectralSensitivity", "子图2.光谱敏感度" },
		{ "Exif.SubImage2.GPSTag", "子图2.GPS标签" },
		{ "Exif.SubImage2.ISOSpeedRatings", "子图2.ISO速度等级" },
		{ "Exif.SubImage2.OECF", "子图2.光电转换函数" },
		{ "Exif.SubImage2.Interlace", "子图2.隔行扫描" },
		{ "Exif.SubImage2.TimeZoneOffset", "子图2.时区偏移" },
		{ "Exif.SubImage2.SelfTimerMode", "子图2.自拍模式" },
		{ "Exif.SubImage2.DateTimeOriginal", "子图2.原始日期时间" },
		{ "Exif.SubImage2.CompressedBitsPerPixel", "子图2.每像素压缩位数" },
		{ "Exif.SubImage2.ShutterSpeedValue", "子图2.快门速度值" },
		{ "Exif.SubImage2.ApertureValue", "子图2.光圈值" },
		{ "Exif.SubImage2.BrightnessValue", "子图2.亮度值" },
		{ "Exif.SubImage2.ExposureBiasValue", "子图2.曝光补偿值" },
		{ "Exif.SubImage2.MaxApertureValue", "子图2.最大光圈值" },
		{ "Exif.SubImage2.SubjectDistance", "子图2.主体距离" },
		{ "Exif.SubImage2.MeteringMode", "子图2.测光模式" },
		{ "Exif.SubImage2.LightSource", "子图2.光源" },
		{ "Exif.SubImage2.Flash", "子图2.闪光灯" },
		{ "Exif.SubImage2.FocalLength", "子图2.焦距" },
		{ "Exif.SubImage2.FlashEnergy", "子图2.闪光灯能量" },
		{ "Exif.SubImage2.SpatialFrequencyResponse", "子图2.空间频率响应" },
		{ "Exif.SubImage2.Noise", "子图2.噪声" },
		{ "Exif.SubImage2.FocalPlaneXResolution", "子图2.焦平面X分辨率" },
		{ "Exif.SubImage2.FocalPlaneYResolution", "子图2.焦平面Y分辨率" },
		{ "Exif.SubImage2.FocalPlaneResolutionUnit", "子图2.焦平面分辨率单位" },
		{ "Exif.SubImage2.ImageNumber", "子图2.图像编号" },
		{ "Exif.SubImage2.SecurityClassification", "子图2.安全分类" },
		{ "Exif.SubImage2.ImageHistory", "子图2.图像历史" },
		{ "Exif.SubImage2.SubjectLocation", "子图2.主体位置" },
		{ "Exif.SubImage2.ExposureIndex", "子图2.曝光指数" },
		{ "Exif.SubImage2.TIFFEPStandardID", "子图2.TIFF/EP标准ID" },
		{ "Exif.SubImage2.SensingMethod", "子图2.感应方法" },
		{ "Exif.SubImage2.XPTitle", "子图2.XP标题" },
		{ "Exif.SubImage2.XPComment", "子图2.XP注释" },
		{ "Exif.SubImage2.XPAuthor", "子图2.XP作者" },
		{ "Exif.SubImage2.XPKeywords", "子图2.XP关键词" },
		{ "Exif.SubImage2.XPSubject", "子图2.XP主题" },
		{ "Exif.SubImage2.PrintImageMatching", "子图2.打印图像匹配" },
		{ "Exif.SubImage2.DNGVersion", "子图2.DNG版本" },
		{ "Exif.SubImage2.DNGBackwardVersion", "子图2.DNG向后兼容版本" },
		{ "Exif.SubImage2.UniqueCameraModel", "子图2.唯一相机型号" },
		{ "Exif.SubImage2.LocalizedCameraModel", "子图2.本地化相机型号" },
		{ "Exif.SubImage2.CFAPlaneColor", "子图2.CFA平面颜色" },
		{ "Exif.SubImage2.CFALayout", "子图2.CFA布局" },
		{ "Exif.SubImage2.LinearizationTable", "子图2.线性化表" },
		{ "Exif.SubImage2.BlackLevelRepeatDim", "子图2.黑电平重复尺寸" },
		{ "Exif.SubImage2.BlackLevel", "子图2.黑电平" },
		{ "Exif.SubImage2.BlackLevelDeltaH", "子图2.水平黑电平增量" },
		{ "Exif.SubImage2.BlackLevelDeltaV", "子图2.垂直黑电平增量" },
		{ "Exif.SubImage2.WhiteLevel", "子图2.白电平" },
		{ "Exif.SubImage2.DefaultScale", "子图2.默认比例" },
		{ "Exif.SubImage2.DefaultCropOrigin", "子图2.默认裁剪原点" },
		{ "Exif.SubImage2.DefaultCropSize", "子图2.默认裁剪尺寸" },
		{ "Exif.SubImage2.ColorMatrix1", "子图2.颜色矩阵1" },
		{ "Exif.SubImage2.ColorMatrix2", "子图2.颜色矩阵2" },
		{ "Exif.SubImage2.CameraCalibration1", "子图2.相机校准1" },
		{ "Exif.SubImage2.CameraCalibration2", "子图2.相机校准2" },
		{ "Exif.SubImage2.ReductionMatrix1", "子图2.缩减矩阵1" },
		{ "Exif.SubImage2.ReductionMatrix2", "子图2.缩减矩阵2" },
		{ "Exif.SubImage2.AnalogBalance", "子图2.模拟平衡" },
		{ "Exif.SubImage2.AsShotNeutral", "子图2.拍摄时的中性点" },
		{ "Exif.SubImage2.AsShotWhiteXY", "子图2.拍摄时的白点XY坐标" },
		{ "Exif.SubImage2.BaselineExposure", "子图2.基准曝光" },
		{ "Exif.SubImage2.BaselineNoise", "子图2.基准噪声" },
		{ "Exif.SubImage2.BaselineSharpness", "子图2.基准锐度" },
		{ "Exif.SubImage2.BayerGreenSplit", "子图2.拜耳绿色分离" },
		{ "Exif.SubImage2.LinearResponseLimit", "子图2.线性响应限制" },
		{ "Exif.SubImage2.CameraSerialNumber", "子图2.相机序列号" },
		{ "Exif.SubImage2.LensInfo", "子图2.镜头信息" },
		{ "Exif.SubImage2.ChromaBlurRadius", "子图2.色度模糊半径" },
		{ "Exif.SubImage2.AntiAliasStrength", "子图2.抗锯齿强度" },
		{ "Exif.SubImage2.ShadowScale", "子图2.阴影比例" },
		{ "Exif.SubImage2.DNGPrivateData", "子图2.DNG私有数据" },
		{ "Exif.SubImage2.CalibrationIlluminant1", "子图2.校准光源1" },
		{ "Exif.SubImage2.CalibrationIlluminant2", "子图2.校准光源2" },
		{ "Exif.SubImage2.BestQualityScale", "子图2.最佳质量比例" },
		{ "Exif.SubImage2.RawDataUniqueID", "子图2.原始数据唯一ID" },
		{ "Exif.SubImage2.OriginalRawFileName", "子图2.原始RAW文件名" },
		{ "Exif.SubImage2.OriginalRawFileData", "子图2.原始RAW文件数据" },
		{ "Exif.SubImage2.ActiveArea", "子图2.有效区域" },
		{ "Exif.SubImage2.MaskedAreas", "子图2.遮罩区域" },
		{ "Exif.SubImage2.AsShotICCProfile", "子图2.拍摄时的ICC配置文件" },
		{ "Exif.SubImage2.AsShotPreProfileMatrix", "子图2.拍摄时的预配置文件矩阵" },
		{ "Exif.SubImage2.CurrentICCProfile", "子图2.当前ICC配置文件" },
		{ "Exif.SubImage2.CurrentPreProfileMatrix", "子图2.当前预配置文件矩阵" },
		{ "Exif.SubImage2.ColorimetricReference", "子图2.比色参考" },
		{ "Exif.SubImage2.CameraCalibrationSignature", "子图2.相机校准签名" },
		{ "Exif.SubImage2.ProfileCalibrationSignature", "子图2.配置文件校准签名" },
		{ "Exif.SubImage2.ExtraCameraProfiles", "子图2.额外相机配置文件" },
		{ "Exif.SubImage2.AsShotProfileName", "子图2.拍摄时配置文件名称" },
		{ "Exif.SubImage2.NoiseReductionApplied", "子图2.已应用的降噪" },
		{ "Exif.SubImage2.ProfileName", "子图2.配置文件名称" },
		{ "Exif.SubImage2.ProfileHueSatMapDims", "子图2.配置文件色相饱和度映射维度" },
		{ "Exif.SubImage2.ProfileHueSatMapData1", "子图2.配置文件色相饱和度映射数据1" },
		{ "Exif.SubImage2.ProfileHueSatMapData2", "子图2.配置文件色相饱和度映射数据2" },
		{ "Exif.SubImage2.ProfileToneCurve", "子图2.配置文件色调曲线" },
		{ "Exif.SubImage2.ProfileEmbedPolicy", "子图2.配置文件嵌入策略" },
		{ "Exif.SubImage2.ProfileCopyright", "子图2.配置文件版权" },
		{ "Exif.SubImage2.ForwardMatrix1", "子图2.前向矩阵1" },
		{ "Exif.SubImage2.ForwardMatrix2", "子图2.前向矩阵2" },
		{ "Exif.SubImage2.PreviewApplicationName", "子图2.预览应用程序名称" },
		{ "Exif.SubImage2.PreviewApplicationVersion", "子图2.预览应用程序版本" },
		{ "Exif.SubImage2.PreviewSettingsName", "子图2.预览设置名称" },
		{ "Exif.SubImage2.PreviewSettingsDigest", "子图2.预览设置摘要" },
		{ "Exif.SubImage2.PreviewColorSpace", "子图2.预览色彩空间" },
		{ "Exif.SubImage2.PreviewDateTime", "子图2.预览日期时间" },
		{ "Exif.SubImage2.RawImageDigest", "子图2.原始图像摘要" },
		{ "Exif.SubImage2.OriginalRawFileDigest", "子图2.原始RAW文件摘要" },
		{ "Exif.SubImage2.SubTileBlockSize", "子图2.子块大小" },
		{ "Exif.SubImage2.RowInterleaveFactor", "子图2.行交错因子" },
		{ "Exif.SubImage2.ProfileLookTableDims", "子图2.配置文件查找表维度" },
		{ "Exif.SubImage2.ProfileLookTableData", "子图2.配置文件查找表数据" },
		{ "Exif.SubImage2.OpcodeList1", "子图2.操作码列表1" },
		{ "Exif.SubImage2.OpcodeList2", "子图2.操作码列表2" },
		{ "Exif.SubImage2.OpcodeList3", "子图2.操作码列表3" },
		{ "Exif.SubImage2.NoiseProfile", "子图2.噪声配置文件" },
		{ "Exif.SubImage2.TimeCodes", "子图2.时间码" },
		{ "Exif.SubImage2.FrameRate", "子图2.帧率" },
		{ "Exif.SubImage2.TStop", "子图2.T档" },
		{ "Exif.SubImage2.ReelName", "子图2.胶片名称" },
		{ "Exif.SubImage2.CameraLabel", "子图2.相机标签" },
		{ "Exif.SubImage2.OriginalDefaultFinalSize", "子图2.原始默认最终尺寸" },
		{ "Exif.SubImage2.OriginalBestQualityFinalSize", "子图2.原始最佳质量最终尺寸" },
		{ "Exif.SubImage2.OriginalDefaultCropSize", "子图2.原始默认裁剪尺寸" },
		{ "Exif.SubImage2.ProfileHueSatMapEncoding", "子图2.配置文件色相饱和度映射编码" },
		{ "Exif.SubImage2.ProfileLookTableEncoding", "子图2.配置文件查找表编码" },
		{ "Exif.SubImage2.BaselineExposureOffset", "子图2.基准曝光偏移" },
		{ "Exif.SubImage2.DefaultBlackRender", "子图2.默认黑色渲染" },
		{ "Exif.SubImage2.NewRawImageDigest", "子图2.新原始图像摘要" },
		{ "Exif.SubImage2.RawToPreviewGain", "子图2.原始到预览增益" },
		{ "Exif.SubImage2.DefaultUserCrop", "子图2.默认用户裁剪" },
		{ "Exif.SubImage2.DepthFormat", "子图2.深度格式" },
		{ "Exif.SubImage2.DepthNear", "子图2.近深度" },
		{ "Exif.SubImage2.DepthFar", "子图2.远深度" },
		{ "Exif.SubImage2.DepthUnits", "子图2.深度单位" },
		{ "Exif.SubImage2.DepthMeasureType", "子图2.深度测量类型" },
		{ "Exif.SubImage2.EnhanceParams", "子图2.增强参数" },
		{ "Exif.SubImage2.ProfileGainTableMap", "子图2.配置文件增益表映射" },
		{ "Exif.SubImage2.SemanticName", "子图2.语义名称" },
		{ "Exif.SubImage2.SemanticInstanceID", "子图2.语义实例ID" },
		{ "Exif.SubImage2.CalibrationIlluminant3", "子图2.校准光源3" },
		{ "Exif.SubImage2.CameraCalibration3", "子图2.相机校准3" },
		{ "Exif.SubImage2.ColorMatrix3", "子图2.颜色矩阵3" },
		{ "Exif.SubImage2.ForwardMatrix3", "子图2.前向矩阵3" },
		{ "Exif.SubImage2.IlluminantData1", "子图2.光源数据1" },
		{ "Exif.SubImage2.IlluminantData2", "子图2.光源数据2" },
		{ "Exif.SubImage2.IlluminantData3", "子图2.光源数据3" },
		{ "Exif.SubImage2.MaskSubArea", "子图2.遮罩子区域" },
		{ "Exif.SubImage2.ProfileHueSatMapData3", "子图2.配置文件色相饱和度映射数据3" },
		{ "Exif.SubImage2.ReductionMatrix3", "子图2.缩减矩阵3" },
		{ "Exif.SubImage2.RGBTables", "子图2.RGB表" },
		{ "Exif.SubImage2.ProfileGainTableMap2", "子图2.配置文件增益表映射2" },
		{ "Exif.SubImage2.ColumnInterleaveFactor", "子图2.列交错因子" },
		{ "Exif.SubImage2.ImageSequenceInfo", "子图2.图像序列信息" },
		{ "Exif.SubImage2.ImageStats", "子图2.图像统计" },
		{ "Exif.SubImage2.ProfileDynamicRange", "子图2.配置文件动态范围" },
		{ "Exif.SubImage2.ProfileGroupName", "子图2.配置文件组名称" },
		{ "Exif.SubImage2.JXLDistance", "子图2.JXL距离" },
		{ "Exif.SubImage2.JXLEffort", "子图2.JXL努力程度" },
		{ "Exif.SubImage2.JXLDecodeSpeed", "子图2.JXL解码速度" },

	};

	std::string getExif(const wstring& path, int width, int height, const uint8_t* buf, int fileSize) {
		static bool isInit = false;
		if (!isInit) {
			isInit = true;
			Exiv2::enableBMFF();
		}

		if (width == 0 || height == 0)
			return std::format("路径: {}\n大小: {}", wstringToUtf8(path), size2Str(fileSize));

		auto res = std::format("路径: {}\n大小: {}\n分辨率: {}x{}",
			wstringToUtf8(path), size2Str(fileSize), width, height);

		try {
			auto image = Exiv2::ImageFactory::open(buf, fileSize);
			image->readMetadata();

			Exiv2::ExifData& exifData = image->exifData();
			if (exifData.empty()) {
				log("No EXIF data {}", wstringToUtf8(path));
				return res;
			}

			for (const auto& item : exifData) {
				res += '\n';
				res += (exifTagsMap.contains(item.key())) ?
					exifTagsMap.at(item.key()) : item.key();
				res += ": ";

				auto&& str = item.value().toString();
				if (str.length() > 40)
					res += str.substr(0, 40)+" ...";
				else
					res += str;
			}
		}
		catch (Exiv2::Error& e) {
			log("Caught Exiv2 exception {} {}", wstringToUtf8(path), e.what());
			return res;
		}
		return res;
	}

	static cv::Mat& getDefaultMat() {
		static cv::Mat defaultMat;
		static bool isInit = false;

		if (!isInit) {
			isInit = true;
			auto rc = GetResource(IDB_PNG, L"PNG");
			std::vector<char> imgData(rc.addr, rc.addr + rc.size);
			defaultMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
		}
		return defaultMat;
	}

	static cv::Mat& getHomeMat() {
		static cv::Mat homeMat;
		static bool isInit = false;

		if (!isInit) {
			isInit = true;
			auto rc = GetResource(IDB_PNG1, L"PNG");
			std::vector<char> imgData(rc.addr, rc.addr + rc.size);
			homeMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
		}
		return homeMat;
	}

	// HEIC ONLY, AVIF not support
	// https://github.com/strukturag/libheif
	// vcpkg install libheif:x64-windows-static
	// vcpkg install libheif[hevc]:x64-windows-static
	cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf, int fileSize) {

		auto exifStr = std::format("路径: {}\n大小: {}",
			wstringToUtf8(path),
			size2Str(fileSize));

		auto filetype_check = heif_check_filetype(buf.data(), 12);
		if (filetype_check == heif_filetype_no) {
			log("Input file is not an HEIF/AVIF file: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		if (filetype_check == heif_filetype_yes_unsupported) {
			log("Input file is an unsupported HEIF/AVIF file type: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		heif_context* ctx = heif_context_alloc();
		auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), fileSize, nullptr);
		if (err.code) {
			log("heif_context_read_from_memory_without_copy error: {} {}", wstringToUtf8(path), err.message);
			return cv::Mat();
		}

		// get a handle to the primary image
		heif_image_handle* handle = nullptr;
		err = heif_context_get_primary_image_handle(ctx, &handle);
		if (err.code) {
			log("heif_context_get_primary_image_handle error: {} {}", wstringToUtf8(path), err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			return cv::Mat();
		}

		// decode the image and convert colorspace to RGB, saved as 24bit interleaved
		heif_image* img = nullptr;
		err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_decode_image error: {}", err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			if (img) heif_image_release(img);
			return cv::Mat();
		}

		int stride = 0;
		const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

		auto width = heif_image_handle_get_width(handle);
		auto height = heif_image_handle_get_height(handle);

		cv::Mat matImg(height, width, CV_8UC4);

		auto srcPtr = (uint32_t*)data;
		auto dstPtr = (uint32_t*)matImg.ptr();
		auto pixelCount = (size_t)width * height;
		for (size_t i = 0; i < pixelCount; ++i) { // ARGB -> ABGR
			uint32_t pixel = srcPtr[i];
			uint32_t r = (pixel >> 16) & 0xFF;
			uint32_t b = pixel & 0xFF;
			dstPtr[i] = (b << 16) | (pixel & 0xff00ff00) | r;
		}

		// clean up resources
		heif_context_free(ctx);
		heif_image_release(img);
		heif_image_handle_release(handle);

		return matImg;
	}

	// vcpkg install libavif[aom]:x64-windows-static libavif[dav1d]:x64-windows-static
	// https://github.com/AOMediaCodec/libavif/issues/1451#issuecomment-1606903425
	cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf, int fileSize) {
		avifImage* image = avifImageCreateEmpty();
		if (image == nullptr) {
			log("avifImageCreateEmpty failure: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		avifDecoder* decoder = avifDecoderCreate();
		if (decoder == nullptr) {
			log("avifDecoderCreate failure: {}", wstringToUtf8(path));
			avifImageDestroy(image);
			return cv::Mat();
		}

		avifResult result = avifDecoderReadMemory(decoder, image, buf.data(), fileSize);
		if (result != AVIF_RESULT_OK) {
			log("avifDecoderReadMemory failure: {} {}", wstringToUtf8(path), avifResultToString(result));
			avifImageDestroy(image);
			avifDecoderDestroy(decoder);
			return cv::Mat();
		}

		avifRGBImage rgb;
		avifRGBImageSetDefaults(&rgb, image);
		result = avifRGBImageAllocatePixels(&rgb);
		if (result != AVIF_RESULT_OK) {
			log("avifRGBImageAllocatePixels failure: {} {}", wstringToUtf8(path), avifResultToString(result));
			avifImageDestroy(image);
			avifDecoderDestroy(decoder);
			return cv::Mat();
		}

		rgb.format = AVIF_RGB_FORMAT_BGRA; // OpenCV is BGRA
		result = avifImageYUVToRGB(image, &rgb);
		if (result != AVIF_RESULT_OK) {
			log("avifImageYUVToRGB failure: {} {}", wstringToUtf8(path), avifResultToString(result));
			avifImageDestroy(image);
			avifDecoderDestroy(decoder);
			avifRGBImageFreePixels(&rgb);
			return cv::Mat();
		}

		avifImageDestroy(image);
		avifDecoderDestroy(decoder);

		auto ret = cv::Mat(rgb.height, rgb.width, CV_8UC4);
		if (rgb.depth == 8) {
			memcpy(ret.ptr(), rgb.pixels, (size_t)rgb.width * rgb.height * 4);
		}
		else {
			log("rgb.depth: {} {}", rgb.depth, wstringToUtf8(path));
			const uint16_t* src = (uint16_t*)rgb.pixels;
			uint8_t* dst = ret.ptr();
			int bitShift = 2;
			switch (rgb.depth) {
			case 10: bitShift = 2; break;
			case 12: bitShift = 4; break;
			case 16: bitShift = 8; break;
			}
			const size_t length = (size_t)rgb.width * rgb.height * 4;
			for (size_t i = 0; i < length; ++i) {
				dst[i] = (uint8_t)(src[i] >> bitShift);
			}
		}

		avifRGBImageFreePixels(&rgb);
		return ret;
	}

	cv::Mat loadRaw(const wstring& path, const vector<uchar>& buf, int fileSize) {
		if (buf.empty() || fileSize <= 0) {
			log("Buf is empty: {} {}", wstringToUtf8(path), fileSize);
			return cv::Mat();
		}

		auto rawProcessor = std::make_unique<LibRaw>();

		int ret = rawProcessor->open_buffer(buf.data(), fileSize);
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot open RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return cv::Mat();
		}

		ret = rawProcessor->unpack();
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot unpack RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return cv::Mat();
		}

		ret = rawProcessor->dcraw_process();
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot process RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return cv::Mat();
		}

		libraw_processed_image_t* image = rawProcessor->dcraw_make_mem_image(&ret);
		if (image == nullptr) {
			log("Cannot make image from RAW data: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return cv::Mat();
		}

		cv::Mat img(image->height, image->width, CV_8UC3, image->data);

		cv::Mat bgrImage;
		cv::cvtColor(img, bgrImage, cv::COLOR_RGB2BGR);

		// Clean up
		LibRaw::dcraw_clear_mem(image);
		rawProcessor->recycle();

		return bgrImage;
	}

	// TODO
	vector<cv::Mat> loadMats(const wstring& path, const vector<uchar>& buf, int fileSize) {
		vector<cv::Mat> imgs;

		if (!cv::imdecodemulti(buf, cv::IMREAD_UNCHANGED, imgs)) {
			return imgs;
		}
		return imgs;
	}

	cv::Mat loadMat(const wstring& path, const vector<uchar>& buf, int fileSize) {
		auto img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);

		if (img.empty()) {
			log("cvMat cannot decode: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		if (img.channels() != 1 && img.channels() != 3 && img.channels() != 4) {
			log("cvMat unsupport channel: {}", img.channels());
			return cv::Mat();
		}

		// enum { CV_8U=0, CV_8S=1, CV_16U=2, CV_16S=3, CV_32S=4, CV_32F=5, CV_64F=6 }
		if (img.depth() <= 1) {
			return img;
		}
		else if (img.depth() <= 3) {
			cv::Mat tmp;
			img.convertTo(tmp, CV_8U, 1.0 / 256);
			return tmp;
		}
		else if (img.depth() <= 5) {
			cv::Mat tmp;
			img.convertTo(tmp, CV_8U, 1.0 / 65536);
			return tmp;
		}
		log("Special: {}, img.depth(): {}, img.channels(): {}",
			wstringToUtf8(path), img.depth(), img.channels());
		return cv::Mat();
	}


	std::vector<ImageNode> loadGif(const wstring& path, const vector<uchar>& buf, size_t size) {

		auto InternalRead_Mem = [](GifFileType* gif, GifByteType* buf, int len) -> int {
			if (len == 0)
				return 0;

			GifData* pData = (GifData*)gif->UserData;
			if (pData->m_nPosition > pData->m_nBufferSize)
				return 0;

			UINT nRead;
			if (pData->m_nPosition + len > pData->m_nBufferSize || pData->m_nPosition + len < pData->m_nPosition)
				nRead = (UINT)(pData->m_nBufferSize - pData->m_nPosition);
			else
				nRead = len;

			memcpy((BYTE*)buf, (BYTE*)pData->m_lpBuffer + pData->m_nPosition, nRead);
			pData->m_nPosition += nRead;

			return nRead;
			};

		std::vector<ImageNode> frames;

		GifData data;
		memset(&data, 0, sizeof(data));
		data.m_lpBuffer = buf.data();
		data.m_nBufferSize = size;

		int error;
		GifFileType* gif = DGifOpen((void*)&data, InternalRead_Mem, &error);

		if (gif == nullptr) {
			log("DGifOpen: Error: {} {}", wstringToUtf8(path), GifErrorString(error));
			frames.push_back({ getDefaultMat(), 0 });
			return frames;
		}

		if (DGifSlurp(gif) != GIF_OK) {
			log("DGifSlurp Error: {} {}", wstringToUtf8(path), GifErrorString(gif->Error));
			DGifCloseFile(gif, &error);
			frames.push_back({ getDefaultMat(), 0 });
			return frames;
		}

		const auto byteSize = 4ULL * gif->SWidth * gif->SHeight;
		auto gifRaster = std::make_unique<GifByteType[]>(byteSize);

		memset(gifRaster.get(), 0, byteSize);

		for (int i = 0; i < gif->ImageCount; ++i) {
			SavedImage& image = gif->SavedImages[i];
			GraphicsControlBlock gcb;
			if (DGifSavedExtensionToGCB(gif, i, &gcb) != GIF_OK) {
				continue;
			}
			auto colorMap = image.ImageDesc.ColorMap != nullptr ? image.ImageDesc.ColorMap : gif->SColorMap;
			auto ptr = gifRaster.get();

			for (int y = 0; y < image.ImageDesc.Height; ++y) {
				for (int x = 0; x < image.ImageDesc.Width; ++x) {
					int gifX = x + image.ImageDesc.Left;
					int gifY = y + image.ImageDesc.Top;
					GifByteType colorIndex = image.RasterBits[y * image.ImageDesc.Width + x];
					if (colorIndex == gcb.TransparentColor) {
						continue;
					}
					GifColorType& color = colorMap->Colors[colorIndex];
					int pixelIdx = (gifY * gif->SWidth + gifX) * 4;
					ptr[pixelIdx] = color.Blue;
					ptr[pixelIdx + 1] = color.Green;
					ptr[pixelIdx + 2] = color.Red;
					ptr[pixelIdx + 3] = 255;
				}
			}

			cv::Mat frame(gif->SHeight, gif->SWidth, CV_8UC4, gifRaster.get());
			cv::Mat cloneFrame;
			frame.copyTo(cloneFrame);
			frames.emplace_back(cloneFrame, gcb.DelayTime * 10);
		}

		DGifCloseFile(gif, nullptr);
		return frames;
	}

	Frames loadImage(const wstring& path) {
		if (path.length() < 4) {
			log("path.length() < 4: {}", wstringToUtf8(path));
			return { {{getDefaultMat(), 0}}, "" };
		}

		auto f = _wfopen(path.c_str(), L"rb");
		if (f == nullptr) {
			log("path canot open: {}", wstringToUtf8(path));
			return { {{getDefaultMat(), 0}}, "" };
		}

		fseek(f, 0, SEEK_END);
		auto fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		vector<uchar> tmp(fileSize, 0);
		fread(tmp.data(), 1, fileSize, f);
		fclose(f);

		auto dotPos = path.rfind(L'.');
		auto ext = (dotPos != std::wstring::npos && dotPos < path.size() - 1) ?
			path.substr(dotPos) : path;
		for (auto& c : ext)	c = std::tolower(c);

		Frames ret;

		if (ext == L".gif") {
			ret.imgList = loadGif(path, tmp, fileSize);
			if (!ret.imgList.empty() && ret.imgList.front().delay >= 0)
				ret.exifStr = getSimpleInfo(path, ret.imgList.front().img.cols, ret.imgList.front().img.rows, tmp.data(), fileSize);
			return ret;
		}

		cv::Mat img;
		if (ext == L".heic" || ext == L".heif") {
			img = loadHeic(path, tmp, fileSize);
		}
		else if (ext == L".avif" || ext == L".avifs") {
			img = loadAvif(path, tmp, fileSize);
		}
		else if (supportRaw.contains(ext)) {
			img = loadRaw(path, tmp, fileSize);
		}

		if (img.empty())
			img = loadMat(path, tmp, fileSize);

		if (ret.exifStr.empty())
			ret.exifStr = getExif(path, img.cols, img.rows, tmp.data(), fileSize);

		if (img.empty())
			img = getDefaultMat();

		ret.imgList.emplace_back(img, 0);

		return ret;
	}

	static HWND getCvWindow(LPCSTR lpWndName) {
		HWND hWnd = (HWND)cvGetWindowHandle(lpWndName);
		if (IsWindow(hWnd)) {
			HWND hParent = GetParent(hWnd);
			DWORD dwPid = 0;
			GetWindowThreadProcessId(hWnd, &dwPid);
			if (dwPid == GetCurrentProcessId()) {
				return hParent;
			}
		}

		return hWnd;
	}

	static void setCvWindowIcon(HINSTANCE hInstance, HWND hWnd, WORD wIconId) {
		if (IsWindow(hWnd)) {
			HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(wIconId));
			SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
	}
}