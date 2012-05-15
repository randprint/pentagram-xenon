/*
 *  Copyright (C) 2002, 2003 The Pentagram Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CONVERTSHAPE_H
#define CONVERTSHAPE_H

class IDataSource;
class ODataSource;

// Convert shape C

/********** Update AutoShapeFormat in shapeconv/ShapeConv.cpp when changed! **********/
struct ConvertShapeFormat
{
	const char *		name;
													//	U8		U8 Gump	U8.SKF	Cru		Cru2D	Pent	Comp
	uint32				len_header;					//	6		6		2		6		6		8		11
	const char *		ident;						//  ""		""		"\2\0"	""		""		"PSHP"	""
	uint32				bytes_ident;				//	0		0		2		0		0		4		0
	uint32				bytes_special;				//	0		0		0		0		0		0		5
	uint32				bytes_header_unk;			//	4		4		0		4		4		0		4
	uint32				bytes_num_frames;			//	2		2		0		2		2		4		2

	uint32				len_frameheader;			//	6		6		0		8		8		8		6
	uint32				bytes_frame_offset;			//	3		3		0		3		3		4		4
	uint32				bytes_frameheader_unk;		//	1		2		0		2		2		0		0
	uint32				bytes_frame_length;			//	2		2		0		3		3		4		2
	uint32				bytes_frame_length_kludge;	//	0		8		0		0		0		0		0

	uint32				len_frameheader2;			//	18		18		10		28		20		20		10
	uint32				bytes_frame_unknown;		//	8		8		0		8		0		0		0
	uint32				bytes_frame_compression;	//	2		2		2		4		4		4		2
	uint32				bytes_frame_width;			//	2		2		2		4		4		4		2
	uint32				bytes_frame_height;			//	2		2		2		4		4		4		2
	uint32				bytes_frame_xoff;			//	2		2		2		4		4		4		2
	uint32				bytes_frame_yoff;			//	2		2		2		4		4		4		2

	uint32				bytes_line_offset;			//	2		2		2		4		4		4		0
	uint32				line_offset_absolute;		//	0		0		0		0		0		1		0
};

// ConvertShapeFrame structure

struct ConvertShapeFrame 
{
	uint8				header_unknown[2];

	uint8				unknown[8];
	uint32				compression;
	sint32				width;
	sint32				height;
	sint32				xoff;
	sint32				yoff;

	uint32				*line_offsets;		// Note these are offsets into rle_data

	sint32				bytes_rle;			// Number of bytes of RLE Data
	uint8				*rle_data;

	void Free()
	{
		delete [] line_offsets;
		line_offsets = 0;

		delete [] rle_data;
		rle_data = 0;
	}

	void Read(IDataSource *source, const ConvertShapeFormat *csf, uint32 frame_length);

	void ReadCmpFrame(IDataSource *source, const ConvertShapeFormat *csf, const uint8 special[256], ConvertShapeFrame *prev);

	void GetPixels(uint8 *buf, sint32 count, sint32 x, sint32 y);
};


// ConvertShape structure

class ConvertShape
{
	uint8				header_unknown[4];
	uint32				num_frames;
	ConvertShapeFrame	*frames;

public:
	ConvertShape() : num_frames(0), frames(0)
	{
	}

	~ConvertShape()
	{
		Free();
	}

	void Free()
	{
		if (frames)
			for(uint32 i = 0; i < num_frames; ++i)
				frames[i].Free();
		
		delete [] frames;
		frames = 0;
		num_frames = 0;
	}


	void Read(IDataSource *source, const ConvertShapeFormat *csf, uint32 real_len);
	void Write(ODataSource *source, const ConvertShapeFormat *csf, uint32 &write_len);

	// This will check to see if a Shape is of a certain type. Return true if ok, false if bad
	static bool Check(IDataSource *source, const ConvertShapeFormat *csf, uint32 real_len);

	// This will also check to see if a shape is of a certain type. However it won't check
	// the rle data, it only checks headers. Return true if ok, false if bad
	static bool CheckUnsafe(IDataSource *source, const ConvertShapeFormat *csf, uint32 real_len);

	// Algorithmically calculate the number of frames
	static int CalcNumFrames(IDataSource *source, const ConvertShapeFormat *csf, uint32 real_len, uint32 start_pos);
};

// Shape format configuration for Pentagram format
extern const ConvertShapeFormat		PentagramShapeFormat;

#endif //CONVERTSHAPE_H
