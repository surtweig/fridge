#include "stdafx.h"
#include "XCM2ROMImageBuilder.h"


typedef struct Section
{
    char* data;
    int size;
    int segments;
} Section;

bool XCM2ROMImageBuilder(vector<string> sectionFiles, string outputFile, ITextStreamWrapper* errstr)
{
    vector<Section> sections;

    bool imagesLoaded = true;
    int totalSegments = 0;
    int totalSections = sectionFiles.size();

    if (totalSections <= XCM2_ROM_MAX_SECTIONS)
    {

        for (int i = 0; i < totalSections; i++)
        {
            ifstream binfile(sectionFiles[i], ios::in | ios::binary | ios::ate);
            if (binfile.is_open())
            {
                Section newsection;

                streampos size = binfile.tellg();
                char* img = new char[size];
                binfile.seekg(0, ios::beg);
                binfile.read(img, size);
                binfile.close();      

                newsection.data = img;
                int segmentsCount = ((int)size - 1) / XCM2_ROM_SEGMENT_SIZE + 1;
                newsection.segments = segmentsCount;
                newsection.size = size;
                sections.push_back(newsection);

                totalSegments += segmentsCount;
                if (totalSegments > XCM2_ROM_MAX_SEGMENTS)
                {
                    errstr->Append("[ERROR] Image exceeds maximum ROM size (")->Append(XCM2_ROM_MAX_SIZE)->Append(" bytes).\n");
                    imagesLoaded = false;
                    break;
                }
                binfile.close();
            }
            else
            {
                errstr->Append( "[ERROR] Cannot open file " )->Append( sectionFiles[i] )->Append("\n");
                imagesLoaded = false;
            }
        }
    }
    else
    {
        errstr->Append( "[ERROR] Sections number exceeds maximum " )->Append( XCM2_ROM_MAX_SECTIONS )->Append( "\n");
        imagesLoaded = false;
    }

    if (imagesLoaded)
    {
        char* toc = new char[XCM2_ROM_TOC_SIZE];
        XCM2_DWORD* dtoc = (XCM2_DWORD*)toc;
        XCM2_DWORD pos = 0;
        for (int i = 0; i < XCM2_ROM_MAX_SECTIONS; i++)
        {
            dtoc[i] = pos;
            if (i < totalSections)
                pos += (XCM2_DWORD)sections[i].segments;
        }

        ofstream romfile(outputFile, ios::out | ios::binary);
        romfile.write(toc, XCM2_ROM_TOC_SIZE);
        delete[] toc;

        for (int i = 0; i < totalSections; i++)
        {
            int sectionSize = sections[i].segments*XCM2_ROM_SEGMENT_SIZE;
            char* img = new char[sectionSize];
            ZeroMemory(img, sectionSize);
            memcpy(img, sections[i].data, sections[i].size);
            romfile.write(img, sectionSize);
        }

        romfile.close();
        errstr->Append( "ROM image file " )->Append( outputFile )->Append( " was successfully generated.\n");

        for (int i = 0; i < totalSections; i++)
            delete[] sections[i].data;
    }
    else
    {
        errstr->Append( "[ERROR] Section files loading failed. ROM image was not generated.\n");
    }

    return imagesLoaded;
}
