#ifndef SRC_FONT_INFO_HPP
#define SRC_FONT_INFO_HPP


template < class TFont, class TU8G2 >
auto ComputeLineHeight (TFont const& i_font, TU8G2& io_u8g2)
{
	io_u8g2.setFont (i_font);

	return io_u8g2.getAscent () - io_u8g2.getDescent () + 2;
};


template < class TFont, class TU8G2 >
auto GetMaxCharWidth (TFont const& i_font, TU8G2& io_u8g2)
{
	io_u8g2.setFont (i_font);

	return io_u8g2.getMaxCharWidth ();
};


#endif // SRC_FONT_INFO_HPP
