/*
 *      Copyright (C) 2012 Edgar Hucek
 *      https://github.com/huceke
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 *  Based on work form : http://www.blackpawn.com/texts/lightmaps/
 *                       https://github.com/lukaszpaczkowski/texture-atlas-generator
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include "Geometry.h"

#include <vector>
#include <list>
#include <string>
#include <map>
#include <sstream>

class CImage
{
  public:
    CImage(std::string filename);
    ~CImage();
    unsigned int GetWidth() { return m_width; };
    unsigned int GetHeight() { return m_height; };
    unsigned int HasAlpha() { return m_alpha; };
    std::string  GetFileName() { return m_filename; };
    ILuint   ImageID() { return m_image; };
  protected:
    std::string m_filename;
    ILuint m_image;
    unsigned int m_width;
    unsigned int m_height;
    unsigned int m_alpha;
    ILuint  m_format;
};

CImage::CImage(std::string filename)
{
  m_filename = filename;

  ilGenImages( 1, & m_image );
  ilBindImage( m_image );

  m_width = 0;
  m_height = 0;
  m_format = 0;
  m_alpha = false;

  if(ilLoadImage( const_cast<char *>( m_filename.c_str() ) ) == IL_TRUE )
  {
    m_width = ilGetInteger(IL_IMAGE_WIDTH);
    m_height = ilGetInteger(IL_IMAGE_HEIGHT);
    m_format = ilGetInteger(IL_IMAGE_FORMAT);

    if ( (m_format == IL_RGBA) || (m_format == IL_BGRA) ) 
    {
      const unsigned char * data = (const unsigned char *)ilGetData() + 3;
      const long size = m_width * m_height;
      for(long i = 0;(i < size) && (!m_alpha); ++i, data += 4)
        m_alpha = (*data != 255);
    }
    iluFlipImage();
  }
  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
}

CImage::~CImage()
{
  ilDeleteImages( 1, &m_image );
}

class CImageNode
{
  public:
    CImageNode(float x1, float y1, float width, float height);
    ~CImageNode();
    bool IsLeaf() { return (m_child[0] == NULL && m_child[1] == NULL); };
    void SetImage(CImage *image) { m_image = image; };
    CImage *GetImage() { return m_image; };
    CImageNode *Insert(CImage *image, int padding);
    CRect Rect() { return m_rect; };
  private:
    CImage      *m_image;
    CRect       m_rect;
    CImageNode  *m_child[2];
};

CImageNode::CImageNode(float x1, float y1, float width, float height)
{
  m_rect.SetRect(x1, y1, x1 + width, y1 + height);
  m_child[0] = NULL;
  m_child[1] = NULL;
  m_image = NULL;

}

CImageNode::~CImageNode()
{
}

CImageNode *CImageNode::Insert(CImage *image, int padding)
{
  if(!IsLeaf())
  {
    // insert at the first child
    CImageNode *newNode = m_child[0]->Insert(image, padding);

    if(newNode != NULL)
      return newNode;

    // insert was not possible. try it at the second child
    return m_child[1]->Insert(image, padding);
  }
  else
  {
    if(m_image != NULL)
      return NULL;

    // image doesnt fit
    if(image->GetWidth() > m_rect.Width() || image->GetHeight() > m_rect.Height())
    {
      return NULL;
    }

    // image fits perfect
    if(image->GetWidth() == m_rect.Width() && image->GetHeight() == m_rect.Height())
    {
      m_image = image;
      return this;
    }

    // lets generate new leafs
    int dw = m_rect.Width()  - image->GetWidth();
    int dh = m_rect.Height() - image->GetHeight();

    if(dw > dh)
    {
      m_child[0] = new CImageNode(m_rect.x1, m_rect.y1, image->GetWidth(), m_rect.Height());
      m_child[1] = new CImageNode(padding + m_rect.x1 + image->GetWidth(), m_rect.y1,
                                  m_rect.Width() - image->GetWidth()- padding, m_rect.Height());
    }
    else
    {
      m_child[0] = new CImageNode(m_rect.x1, m_rect.y1, m_rect.Width(), image->GetHeight());
      m_child[1] = new CImageNode(m_rect.x1, padding + m_rect.y1 + image->GetHeight(),
                                  m_rect.Width(), m_rect.Height() - image->GetHeight() - padding);
    }

    // insert at the first cwchildleaf
    return m_child[0]->Insert(image, padding);
  }
}

class CImageTree
{
  public:
    CImageTree(unsigned int width, unsigned int height);
    ~CImageTree();
    bool Insert(CImage *image, int padding);
    void Print();
    void SaveImage(std::string filename);
    void SaveIndex(std::string filename);
  private:
    CImageNode *m_root;
    unsigned int m_width;
    unsigned int m_height;
    std::map<std::string, CImageNode *> m_treeMap;

    ILuint m_image;
};

CImageTree::CImageTree(unsigned int width, unsigned int height)
{
  m_width = width;
  m_height = height;
  m_root = new CImageNode(0,0, m_width, m_height);

  ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
  ilGenImages( 1, &m_image );
  ilBindImage( m_image );
  ilTexImage( m_width, m_height, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, 0 );
  ilClearImage();
}

void CImageTree::SaveImage(std::string filename)
{
  ilBindImage( m_image );
  iluFlipImage();
  ilEnable( IL_FILE_OVERWRITE );
  ilSaveImage( const_cast<char*>( filename.c_str() ) );
}

void CImageTree::SaveIndex(std::string filename)
{
  FILE *fp = fopen(filename.c_str(), "w");

  for( std::map<std::string, CImageNode *>::const_iterator i = m_treeMap.begin(); i != m_treeMap.end(); ++i )
  {
    std::string name = (*i).first;
    CImageNode *node = (*i).second;
    if(!node)
      continue;

    CRect rect = node->Rect();

    fprintf(fp, "%s %d %d %d %d\n", name.c_str(), 
           (int)rect.x1, (int)rect.y1, (int)(rect.x2 - rect.x1), (int)(rect.y2 - rect.y1));
  }
  fclose(fp);
}

using namespace std;

CImageTree::~CImageTree()
{
  ilDeleteImages( 1, &m_image );
}

bool CImageTree::Insert(CImage *image, int padding)
{
  if(image == NULL)
    return false;

  if(m_treeMap[image->GetFileName()] != 0)
  {
    printf("dupplicate image : %s\n", image->GetFileName().c_str());
    return false;
  }

  CImageNode *node = m_root->Insert(image, padding);

  if(node == NULL)
    return false;

  m_treeMap[image->GetFileName()] = node;

  ilBindImage( image->ImageID() );
  void * buf = ilGetData();
  ILuint format = ilGetInteger(IL_IMAGE_FORMAT);
  ILuint type = ilGetInteger(IL_IMAGE_TYPE);

  ilBindImage( m_image );
  ilSetPixels( node->Rect().x1 , node->Rect().y1 , 0, image->GetWidth(), image->GetHeight(), 1, format, type, buf );

  return true;
}

void CImageTree::Print()
{
  for( std::map<std::string, CImageNode *>::const_iterator i = m_treeMap.begin(); i != m_treeMap.end(); ++i )
  {
    std::string name = (*i).first;
    CImageNode *node = (*i).second;
    if(!node)
      continue;

    CRect rect = node->Rect();

    printf("%s %d %d %d %d\n", name.c_str(), 
           (int)rect.x1, (int)rect.y1, (int)(rect.x2 - rect.x1), (int)(rect.y2 - rect.y1));
  }
}

using namespace std;

std::vector<std::string> GetFiles(const std::string & path, std::vector<std::string> &files)
{
  struct dirent **list;
  int n;
  struct stat st;
  std::string src_dir_name = ( path[ path.length() - 1 ] != '/' ) ? path + "/" : path;

  n = scandir( src_dir_name.c_str(), &list, 0, alphasort );
  if ( n < 1 ) 
  {
    files.clear();
    return files;
  }

  for( --n; n >= 0; n-- ) {
    std::string file = list[n]->d_name;

    free( list[n] );

    stat( std::string(src_dir_name + file).c_str(), &st );
    if ( S_ISDIR(st.st_mode) )
    {
      if(file != "." && file != "..")
      {
        GetFiles(src_dir_name + file, files);
      }
    }
    else
    {
      files.push_back( src_dir_name + file );
    }
  }
  free( list );
  return files;
}

#define ATLAS_WIDTH   2048
#define ATLAS_HEIGHT  2048

int main(int argc, char *argv[])
{
  std::vector<std::string> files;

  if(argc < 2)
    return 0;

  GetFiles(std::string(argv[1]), files);

  if(files.size() < 1)
    return 0;

  ilInit();
  iluInit();
  ilEnable( IL_ORIGIN_SET );
  ilOriginFunc( IL_ORIGIN_UPPER_LEFT );
  ilEnable( IL_TYPE_SET );
  ilSetInteger( IL_TYPE_MODE, IL_UNSIGNED_BYTE );

  std::list<CImageTree *> m_imageTree;

  CImageTree *imageTree = new CImageTree(ATLAS_WIDTH, ATLAS_HEIGHT);

  m_imageTree.push_back(imageTree);

  std::vector<CImage *> m_imageList;

  for(std::vector<std::string>::const_iterator ifiles = files.begin(); ifiles != files.end(); ++ifiles )
  {
    std::string file = (*ifiles);

    CImage *image = new CImage(file);

    // no image
    if(image->GetWidth() == 0 || image->GetHeight() == 0)
    {
      delete image;
      continue;
    }

    // add image by size in image list
    bool nAdd = false;
    for(std::vector<CImage *>::iterator iimage = m_imageList.begin(); iimage != m_imageList.end(); ++iimage )
    {
      CImage *image_check = (*iimage);

      if((image->GetWidth() * image->GetHeight()) > (image_check->GetWidth() * image_check->GetHeight()))
      {
        m_imageList.insert(iimage--, image);
        nAdd = true;
        break;
      }
    }

    if(!nAdd)
      m_imageList.push_back(image);
  }

  for(std::vector<CImage *>::const_iterator iimage = m_imageList.begin(); iimage != m_imageList.end(); ++iimage )
  {
    CImage *image = (*iimage);

    // image does not fit in map at all
    if(image->GetWidth() > ATLAS_WIDTH || image->GetHeight() > ATLAS_HEIGHT)
    {
      printf("Image : %s to big\n", image->GetFileName().c_str());
      continue;
    }

    // insert image in map
    for(std::list<CImageTree *>::const_iterator itree = m_imageTree.begin(); itree != m_imageTree.end(); ++itree)
    {
      CImageTree *imageTree = (*itree);
      if(imageTree->Insert(image, 0))
      {
        image = NULL;
        break;
      }
    }

    // start a new map if the image was not added to the existing map's
    if(image != NULL)
    {
      CImageTree *imageTree = new CImageTree(ATLAS_WIDTH, ATLAS_HEIGHT);
      m_imageTree.push_back(imageTree);
      if(!imageTree->Insert(image, 0))
        printf("File : %s does not fit\n", image->GetFileName().c_str());
    }
  }

  int count = 0;
  for( std::list<CImageTree *>::const_iterator itree = m_imageTree.begin(); itree != m_imageTree.end(); ++itree )
  {
    CImageTree *imageTree = (*itree);
    std::stringstream ss;
    ss << count;
    std::string filename = "media-2048-" + ss.str() + ".png";
    imageTree->SaveImage(filename);
    filename = "media-2048-" + ss.str() + ".txt";
    imageTree->SaveIndex(filename);
    imageTree->Print();
    count++;
  }

  // TODO: clean memory
  ilShutDown();

  return 1;
}
