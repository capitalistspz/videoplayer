/*****************************************************************
|
|    AP4 - MP4 to AVC File Converter
|
|    Copyright 2002-2009 Axiomatic Systems, LLC
|
|
|    This file is part of Bento4/AP4 (MP4 Atom Processing Library).
|
|    Unless you have obtained Bento4 under a difference license,
|    this version of Bento4 is Bento4|GPL.
|    Bento4|GPL is free software; you can redistribute it and/or modify
|    it under the terms of the GNU General Public License as published by
|    the Free Software Foundation; either version 2, or (at your option)
|    any later version.
|
|    Bento4|GPL is distributed in the hope that it will be useful,
|    but WITHOUT ANY WARRANTY; without even the implied warranty of
|    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|    GNU General Public License for more details.
|
|    You should have received a copy of the GNU General Public License
|    along with Bento4|GPL; see the file COPYING.  If not, write to the
|    Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
|    02111-1307, USA.
|
 ****************************************************************/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <bento4/Ap4FileByteStream.h>
#include <bento4/Ap4AvcParser.h>
#include <bento4/Ap4Track.h>
#include <bento4/Ap4Types.h>
#include <bento4/Ap4SampleDescription.h>
#include <bento4/Ap4Sample.h>
#include <bento4/Ap4File.h>
#include <bento4/Ap4Movie.h>
#include <coreinit/debug.h>
#include <whb/log.h>
#include "MP4.h"

#include <algorithm>

/*----------------------------------------------------------------------
|   WriteSample
+---------------------------------------------------------------------*/
static void
WriteSample(const AP4_DataBuffer& sample_data,
            AP4_DataBuffer&       prefix,
            unsigned int          nalu_length_size,
            std::vector<uint8_t>&       output)
{
    const unsigned char* data      = sample_data.GetData();
    unsigned int         data_size = sample_data.GetDataSize();

    // allocate a buffer for the PES packet
    AP4_DataBuffer frame_data;
    unsigned char* frame_buffer = NULL;

    // add a delimiter if we don't already have one
    bool have_access_unit_delimiter = (data_size >  nalu_length_size) && ((data[nalu_length_size] & 0x1F) == AP4_AVC_NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER);
    if (!have_access_unit_delimiter) {
        AP4_Size frame_data_size = frame_data.GetDataSize();
        frame_data.SetDataSize(frame_data_size+6);
        frame_buffer = frame_data.UseData()+frame_data_size;

        // start of access unit
        frame_buffer[0] = 0;
        frame_buffer[1] = 0;
        frame_buffer[2] = 0;
        frame_buffer[3] = 1;
        frame_buffer[4] = 9;    // NAL type = Access Unit Delimiter;
        frame_buffer[5] = 0xE0; // Slice types = ANY
    }

    // write the NAL units
    bool prefix_added = false;
    while (data_size) {
        // sanity check
        if (data_size < nalu_length_size) break;

        // get the next NAL unit
        AP4_UI32 nalu_size;
        if (nalu_length_size == 1) {
            nalu_size = *data++;
            data_size--;
        } else if (nalu_length_size == 2) {
            nalu_size = AP4_BytesToInt16BE(data);
            data      += 2;
            data_size -= 2;
        } else if (nalu_length_size == 4) {
            nalu_size = AP4_BytesToInt32BE(data);
            data      += 4;
            data_size -= 4;
        } else {
            break;
        }
        if (nalu_size > data_size) break;

        // add the prefix if needed
        if (prefix.GetDataSize() && !prefix_added && !have_access_unit_delimiter) {
            AP4_Size frame_data_size = frame_data.GetDataSize();
            frame_data.SetDataSize(frame_data_size+prefix.GetDataSize());
            frame_buffer = frame_data.UseData()+frame_data_size;
            AP4_CopyMemory(frame_buffer, prefix.GetData(), prefix.GetDataSize());
            prefix_added = true;
        }

        // add a start code before the NAL unit
        AP4_Size frame_data_size = frame_data.GetDataSize();
        frame_data.SetDataSize(frame_data_size+3+nalu_size);
        frame_buffer = frame_data.UseData()+frame_data_size;
        frame_buffer[0] = 0;
        frame_buffer[1] = 0;
        frame_buffer[2] = 1;
        AP4_CopyMemory(frame_buffer+3, data, nalu_size);

        // add the prefix if needed
        if (prefix.GetDataSize() && !prefix_added) {
            AP4_Size frame_data_size = frame_data.GetDataSize();
            frame_data.SetDataSize(frame_data_size+prefix.GetDataSize());
            frame_buffer = frame_data.UseData()+frame_data_size;
            AP4_CopyMemory(frame_buffer, prefix.GetData(), prefix.GetDataSize());
            prefix_added = true;
        }

        // move to the next NAL unit
        data      += nalu_size;
        data_size -= nalu_size;
    }

    std::copy_n(frame_data.GetData(), frame_data.GetDataSize(), std::back_inserter(output));
}

/*----------------------------------------------------------------------
|   MakeFramePrefix
+---------------------------------------------------------------------*/
static AP4_Result
MakeFramePrefix(AP4_SampleDescription* sdesc, AP4_DataBuffer& prefix, unsigned int& nalu_length_size)
{
    AP4_AvcSampleDescription* avc_desc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sdesc);
    if (avc_desc == nullptr) {
        WHBLogPrintf( "ERROR: track does not contain an AVC stream\n");
        return AP4_FAILURE;
    }

    const auto descFormat=  sdesc->GetFormat();
    if (descFormat == AP4_SAMPLE_FORMAT_AVC3 ||
        descFormat == AP4_SAMPLE_FORMAT_AVC4 ||
        descFormat == AP4_SAMPLE_FORMAT_DVAV) {
        // no need for a prefix, SPS/PPS NALs should be in the elementary stream already
        return AP4_SUCCESS;
    }

    // make the SPS/PPS prefix
    nalu_length_size = avc_desc->GetNaluLengthSize();
    for (unsigned int i=0; i<avc_desc->GetSequenceParameters().ItemCount(); i++) {
        AP4_DataBuffer& buffer = avc_desc->GetSequenceParameters()[i];
        unsigned int prefix_size = prefix.GetDataSize();
        prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
        unsigned char* p = prefix.UseData()+prefix_size;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 1;
        AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
    }
    for (unsigned int i=0; i<avc_desc->GetPictureParameters().ItemCount(); i++) {
        AP4_DataBuffer& buffer = avc_desc->GetPictureParameters()[i];
        unsigned int prefix_size = prefix.GetDataSize();
        prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
        unsigned char* p = prefix.UseData()+prefix_size;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 1;
        AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
    }

    return AP4_SUCCESS;
}


/*----------------------------------------------------------------------
|   WriteSamples
+---------------------------------------------------------------------*/
static void
WriteSamples(AP4_Track*             track,
             AP4_SampleDescription* sdesc,
             std::vector<uint8_t>&        output)
{
    // make the frame prefix
    unsigned int   nalu_length_size = 0;
    AP4_DataBuffer prefix;
    if (AP4_FAILED(MakeFramePrefix(sdesc, prefix, nalu_length_size))) {
        WHBLogPrint("Failed to make frame prefix");
        return;
    }

    AP4_Sample     sample;
    AP4_DataBuffer data;
    AP4_Ordinal    index = 0;
    while (AP4_SUCCEEDED(track->ReadSample(index, sample, data))) {
        WriteSample(data, prefix, nalu_length_size, output);
	    index++;
    }
    WHBLogPrintf("Wrote %d samples", index);
}
// 16.16 fixed to 32 bit float
float fixed_to_floating_pt(uint32_t val) {
    return (val >> 16) + (val & 0xffff) / 65536.0f;
}
/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
bool LoadAVCTrackFromMP4(const std::filesystem::path& path, std::vector<uint8_t>& byteStream, unsigned& outWidth, unsigned& outHeight)
{

    AP4_Result result;

	// create the input stream
    AP4_ByteStream* input = nullptr;
    result = AP4_FileByteStream::Create(path.c_str(), AP4_FileByteStream::STREAM_MODE_READ, input);
    if (AP4_FAILED(result)) {
        WHBLogPrintf( "ERROR: cannot open input (%d)\n", result);
        return false;
    }

	// open the file
    auto input_file = std::make_unique<AP4_File>(*input);

    // get the movie
    AP4_Movie* movie = input_file->GetMovie();
    if (movie == nullptr) {
        WHBLogPrintf( "ERROR: no movie in file\n");
        input->Release();
        return false;
    }

    // get the video track
    AP4_Track *video_track = movie->GetTrack(AP4_Track::TYPE_VIDEO);
    if (video_track == nullptr) {
        WHBLogPrintf( "ERROR: no video track found\n");
        input->Release();
        return false;
    }

    // check that the track is of the right type
    AP4_SampleDescription *sample_description = video_track->GetSampleDescription(0);
    if (sample_description == nullptr) {
        WHBLogPrintf("ERROR: unable to parse sample description\n");
        input->Release();
        return false;
    }

    outWidth =  fixed_to_floating_pt(video_track->GetWidth());
    outHeight = fixed_to_floating_pt(video_track->GetHeight());
    // show info
    AP4_Debug("Video Track:\n");
    AP4_Debug("  duration: %u ms\n",  (int)video_track->GetDurationMs());
    AP4_Debug("  sample count: %u\n", (int)video_track->GetSampleCount());

    switch (sample_description->GetType()) {
        case AP4_SampleDescription::TYPE_AVC:
            WHBLogPrint("Writing samples");
            WriteSamples(video_track, sample_description, byteStream);
            break;

        case AP4_SampleDescription::TYPE_PROTECTED:
            OSFatal("No support for protected video");
            break;

        default:
            WHBLogPrintf( "ERROR: unsupported sample type\n");
            break;
    }

    input->Release();
    return true;
}

