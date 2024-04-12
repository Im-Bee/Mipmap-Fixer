#include <iostream>
#include <exception>
#include <fstream>
#include <filesystem>

#include <../External/Lodepng/lodepng.h>

#define MF_DEFAULT_IN_PATH	L".\\In\\"
#define MF_DEFAULT_OUT_PATH L".\\Out\\"

#define MF_THROW_ERROR(e) if (e != 0) { std::cout << "Error: " << lodepng_error_text(e) << endl; throw exception(lodepng_error_text(e));	}

using namespace std;

struct Arguments
{
	wstring InDir  = wstring();
	wstring OutDir = wstring();
	bool NoLastSlice = false;
};

struct Image
{
	vector<unsigned char> RawPng = {};
	vector<unsigned char> Pixels = {};
	unsigned 
		Width = 0, 
		Height = 0;
	lodepng::State State = {};
};

void DisplayMsgAndExit(char const* const msg)
{
	std::cout << "Something went wrong, program will exit" << endl;
	std::cout << msg << endl;

	exit(-1);
}

inline wstring ConvertCharPtrToWString(char const* const str)
{
	string myStr = str;
	return wstring(myStr.begin(), myStr.end());
}

inline string ConvertWCharPtrToString(wchar_t const* const str)
{
	wstring myWStr = str;
	return string(myWStr.begin(), myWStr.end());
}

inline void NormalizePathArgument(wstring& wstr)
{
	static vector<char> bannedChars = {
		'"',
		'\''
	};

	for (int i = 0; i < wstr.size(); ++i)
	{
		if (find(bannedChars.begin(), bannedChars.end(), wstr[i]) != bannedChars.end())
			wstr.erase(wstr[i]);
	}
}

Arguments ProcessArgs(int argc, char* argv[])
{
	Arguments myArgs = {};

	// Skip the first args
	for (int i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			// If contains that letter then set this option
			if (strchr(argv[i], 'n') != NULL)
			{
				myArgs.NoLastSlice = true;
			}

			continue;
		}
		if (myArgs.InDir.empty())
		{
			myArgs.InDir = ConvertCharPtrToWString(argv[i]);
			NormalizePathArgument(myArgs.InDir);
			if (myArgs.InDir.back() != '\\' &&
				myArgs.InDir.back() != '/')
				myArgs.InDir += '\\';

			continue;
		}
		if (myArgs.OutDir.empty())
		{
			myArgs.OutDir = ConvertCharPtrToWString(argv[i]);
			NormalizePathArgument(myArgs.OutDir);
			if (myArgs.OutDir.back() != '\\' &&
				myArgs.OutDir.back() != '/')
				myArgs.OutDir += '\\';

			continue;
		}
	}

	// Set the most important arguments
	if (myArgs.InDir.empty())
	{
		myArgs.InDir = MF_DEFAULT_IN_PATH;
	}
	if (myArgs.OutDir.empty())
	{
		myArgs.OutDir = MF_DEFAULT_OUT_PATH;
	}

	return myArgs;
}

Image ExtractRawPixelsFromPNG(wchar_t const* const pngPath)
{
	// Check if exists
	fstream myFile = fstream(pngPath, ios_base::in | ios_base::binary);
	if (!myFile.is_open())
	{
		DisplayMsgAndExit("Couldn't open the file");
	}
	myFile.close();

	Image img = {};
	string stringifiedFilePath = ConvertWCharPtrToString(pngPath);

	int error = lodepng::load_file(img.RawPng, stringifiedFilePath);
	MF_THROW_ERROR(error);

	error = lodepng::decode(img.Pixels, img.Width, img.Height, img.State, img.RawPng);
	MF_THROW_ERROR(error);

	return img;
}

void FixMipmapsHeight(Image& img, const bool& noLastSlice)
{
	Image* pFront = &img;
	Image* pBack = new Image(img);

	Image* pTmp = nullptr;
	uint64_t pixW = img.Pixels.size() / img.Height;
	uint64_t frntPixLvl, bckPixelLevel;
	const uint64_t frntBuffHalf = img.Pixels.size() / 2;
	while (pFront->Height > pFront->Width)
	{
		for (uint64_t h = 0; h < img.Height / 2; ++h)
		{
			frntPixLvl = pixW * h;
			bckPixelLevel = pixW * 2 * h;
			for (uint64_t i = 0; i < pixW; ++i)
			{
				pBack->Pixels[bckPixelLevel + i] = pFront->Pixels[frntPixLvl + i];
			}
			for (uint64_t i = 0; i < pixW; ++i)
			{
				pBack->Pixels[bckPixelLevel + i + pixW] = pFront->Pixels[frntPixLvl + i + frntBuffHalf];
			}
		}

		pBack->Height = pFront->Height / 2;
		pBack->Width = pFront->Width * 2;

		pTmp = pFront;
		pFront = pBack;
		pBack = pTmp;

		if (noLastSlice)
		{
			// Check if next slice will end the loop
			// if true then break
			if (pFront->Height / 2 <= pFront->Width * 2)
				break;
		}
	}
	
	img = *pFront;
}

void SavePixels(Image& img, const string& path)
{
	Image myFile = {};
	int error = lodepng::encode(myFile.RawPng, img.Pixels, img.Width, img.Height, myFile.State);
	MF_THROW_ERROR(error);

	lodepng::save_file(myFile.RawPng, path);
}

int main(int argc, char* argv[])
{
	Arguments myArgs = ProcessArgs(argc, argv);

	if (!filesystem::exists(myArgs.InDir))
	{
		string myInPath = ConvertWCharPtrToString(myArgs.InDir.c_str());
		std::cout << "Provided in directory didn't exist: " << myInPath << endl;

		// Ask user if he wants to create the directory and create it
		std::cout << "Do you want to create it? [y/n] ";
		char userAnswer[64] = { 0 };
		cin >> userAnswer;
		if (tolower(userAnswer[0]) == 'y')
		{
			if (filesystem::create_directory(myArgs.InDir))
				std::cout << "Directory has been created" << endl;
		}
	}

	bool checkOut = false;
	vector<string> pathsOfExtractedPngs = {};
	for (auto& file : filesystem::directory_iterator(myArgs.InDir))
	{
		if (file.is_directory()) 
			continue;

		auto myImg = ExtractRawPixelsFromPNG(file.path().native().c_str());

		std::cout << "Raw og: " << lodepng_get_raw_size(myImg.Width, myImg.Height, &myImg.State.info_raw);
		std::cout << " Pixels: " << myImg.Pixels.size() << endl;

		if (!checkOut &&
			!filesystem::exists(myArgs.OutDir))
		{
			string myInPath = ConvertWCharPtrToString(myArgs.OutDir.c_str());
			std::cout << "Provided output directory didn't exist: " << myInPath << endl;

			// Ask user if he wants to create the directory and create it
			std::cout << "Do you want to create it? [y/n] ";
			char userAnswer[64] = { 0 };
			cin >> userAnswer;
			if (tolower(userAnswer[0]) == 'y')
			{
				if (filesystem::create_directory(myArgs.OutDir))
					std::cout << "Directory has been created" << endl;
			}

			// Out directory still doesn't exists 
			if (!filesystem::exists(myArgs.OutDir))
				DisplayMsgAndExit("The output directory didn't exist, couldn't save the files");

			checkOut = true;
		}

		FixMipmapsHeight(myImg, myArgs.NoLastSlice);

		std::cout << "Raw fixed: " << lodepng_get_raw_size(myImg.Width, myImg.Height, &myImg.State.info_raw);
		std::cout << " Pixels: " << myImg.Pixels.size() << endl;

		string savePath = ConvertWCharPtrToString(myArgs.OutDir.c_str());
		savePath += file.path().filename().string();
		SavePixels(myImg, savePath.c_str());
	}
}