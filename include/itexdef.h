/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if !defined (INCLUDED_ITEXDEF_H)
#define INCLUDED_ITEXDEF_H

class Matrix4;

/* greebo: The texture definition structure, containing the scale, 
 * rotation and shift values of an applied texture. 
 * At some places this is referred to as "fake" texture coordinates.
 */
class GenericTextureDefinition {
public:
	float	shift[2];
	float	rotate;
	float	scale[2];

	// Test the texture definition for insanely large values
	virtual bool isSane() const = 0;
	virtual Matrix4 getTransform(float width, float height) const = 0;
};

#endif
