// Copyright [2014] <cuizhaohui>
/**
 * \file i18nText.h
 * \author CUI Zhao-hui, zhh.cui@gmail.com
 * \date 2014-07-01
 * \brief Define a class for puting i18n characters on an image. "FreeType library" is used here.
 */

#ifndef __ZHHCUI_I18NTEXT__
#define __ZHHCUI_I18NTEXT__
//#ifdef __cplusplus

#include <opencv2/core.hpp>

namespace cv {
namespace i18ntext{

class CV_EXPORTS_W i18nText {
  public:
	  CV_WRAP virtual ~i18nText(){};

/** @brief check the status of loading font.

The function isValid is to query the status of font loading

@param 
@return true means font is successful to load. false means font not loading
*/
    CV_WRAP virtual bool isValid() = 0;
    
/** @brief Load font data.

The function setFont loads font data.

@param name FontFile Name
@return true means success, false means failure.
*/ 

    CV_WRAP virtual bool setFont(const char *name) = 0;
/** @brief set font shape

The function setSize set font shape

@param pixelSize is font size, 
@param space is line width of font, 
@param gap is gap between characters.

@return true means success, false means failure.
*/   
    CV_WRAP virtual void setSize(int pixelSize, double space, double gap) = 0;
    
/** @brief Draws a text string.

The function createi18nText renders the specified text string in the image. Symbols that cannot be rendered using the specified font are replaced by "Tofu" or non-drawn.

@param img Image.
@param text Text string to be drawn, this string should be in wideChar format. The real type of this parameter is wchar_t *.
@param pos Bottom-left/Top-left corner of the text string in the image.
@param color Text color

@return the number of drawn character
.
*/    
    CV_WRAP virtual int putWText(InputOutputArray &img, const char *text, Point pos, Scalar color) = 0;
};


CV_EXPORTS_W Ptr<i18nText> createi18nText();
	
}}	//namespace i18ntext
//#endif
#endif

