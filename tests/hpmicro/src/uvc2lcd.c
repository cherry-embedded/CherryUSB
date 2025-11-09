#include "board.h"
#include "hpm_lcdc_drv.h"
#include "hpm_l1c_drv.h"
#include "usbh_uvc_stream.h"

#define LCD          BOARD_LCD_BASE
#define PIXEL_FORMAT display_pixel_format_ycbcr422

#define IMAGE_WIDTH  640
#define IMAGE_HEIGHT 480

static ATTR_PLACE_AT_WITH_ALIGNMENT(".framebuffer", 64) uint8_t frame_buffer1[IMAGE_WIDTH * IMAGE_HEIGHT * 2];
static ATTR_PLACE_AT_WITH_ALIGNMENT(".framebuffer", 64) uint8_t frame_buffer2[IMAGE_WIDTH * IMAGE_HEIGHT * 2];
static struct usbh_videoframe frame_pool[2];

void writefont2screen(uint16_t or_x, uint16_t or_y, uint16_t x_end, uint16_t y_end, uint8_t assic_id, uint16_t colour,
                       uint8_t clearflag, uint8_t *str_font, uint32_t screen_addr, uint16_t font_size)
{
    uint8_t *strdisp;
    uint16_t x, y;
    uint8_t bit;
    uint8_t temp1;
    strdisp = (uint8_t *)screen_addr;
    str_font += font_size * (assic_id - 0x20); /*get end encode*/
    bit = 0;
    for (y = or_y; y <= y_end; y++) {
        for (x = or_x; x <= x_end; x++) {
            if (clearflag == true) {
                *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x) = colour & 0x00ff;
                *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x + 1) = colour >> 8;
            } else {
                temp1 = (*str_font) >> bit;
                if ((temp1 & 0x01) == 0x01) {
                    *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x) = colour & 0x00ff;
                    *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x + 1) = colour >> 8;
                } else {
                    *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x) = 0;
                    *(strdisp + y * (IMAGE_WIDTH * 2) + 2 * x + 1) = 0;
                }
                bit++;
                if (bit == 8) {
                    bit = 0;
                    str_font += 1;
                }
            }
        }
    }
}

char string2font(uint16_t line, uint16_t column, uint8_t *string, uint8_t string_num, uint16_t colour,
                  uint8_t *str_font, uint32_t screen_addr, uint8_t font_width, uint8_t font_height)
{
    uint8_t i = 0, j = 0, numtemp = 0;
    uint16_t or_x, or_y, x_end, y_end;
    uint16_t font_stroage_size;
    or_x = column * font_width;
    or_y = line * font_height;
    x_end = or_x + font_width - 1;
    y_end = or_y + font_height - 1;
    font_stroage_size = font_width * font_height / 8;
    for (numtemp = 0; numtemp < string_num; numtemp++) {
        if ((*(string + numtemp) != 10) && (*(string + numtemp) != 0)) { /*enter or end*/
            if (*(string + numtemp) != 8) {                              /*delete*/
                writefont2screen(or_x + font_width * i, or_y + font_height * j, x_end + font_width * i, y_end + font_height * j,
                                  *(string + numtemp), colour, false, str_font, screen_addr, font_stroage_size);
            } else {
                writefont2screen(or_x + font_width * i, or_y + font_height * j, x_end + font_width * i, y_end + font_height * j,
                                  *(string + numtemp), colour, true, str_font, screen_addr, font_stroage_size);
            }
        } else if (*(string + numtemp) == 10) {
            i = 19; /* jump next line */
        } else if (*(string + numtemp) == 0) {
            return true;
        }
        i++;
        if (i * font_width == IMAGE_WIDTH) {
            j++;
            i = 0;
        }
    }
    return true;
}

extern const unsigned char nAsciiDot24x48[];
extern volatile uint32_t g_uvc_fps;

void usbh_video_run(struct usbh_video *video_class)
{
    usbh_video_stream_start(640, 480, USBH_VIDEO_FORMAT_UNCOMPRESSED);
    lcdc_turn_on_display(LCD);
}

void usbh_video_stop(struct usbh_video *video_class)
{
    usbh_video_stream_stop();
    lcdc_turn_off_display(LCD);
}

void usbh_video_frame_callback(struct usbh_videoframe *frame)
{
    char font_display_buf[50];

    //USB_LOG_RAW("frame buf:%p,frame len:%d\r\n", frame->frame_buf, frame->frame_size);
    l1c_dc_invalidate((uint32_t)frame->frame_buf, IMAGE_WIDTH * IMAGE_HEIGHT * 2);
    sprintf(font_display_buf, "fps:%d", g_uvc_fps);
    string2font(1, 1, (uint8_t *)font_display_buf, sizeof(font_display_buf), 0x001f, (uint8_t *)nAsciiDot24x48, (uint32_t)frame->frame_buf, 24, 48);
    l1c_dc_writeback((uint32_t)frame->frame_buf, IMAGE_WIDTH * IMAGE_HEIGHT * 2);
    lcdc_layer_set_next_buffer(LCD, 0, (uint32_t)frame->frame_buf);
}

void init_lcd(void)
{
    uint8_t layer_index = 0;
    lcdc_config_t config = { 0 };
    lcdc_layer_config_t layer = { 0 };

    lcdc_get_default_config(LCD, &config);
    board_panel_para_to_lcdc(&config);
    lcdc_init(LCD, &config);

    lcdc_get_default_layer_config(LCD, &layer, PIXEL_FORMAT, layer_index);

    layer.position_x = (BOARD_LCD_WIDTH - IMAGE_WIDTH) / 2;
    layer.position_y = (BOARD_LCD_HEIGHT - IMAGE_HEIGHT) / 2;
    layer.width = IMAGE_WIDTH;
    layer.height = IMAGE_HEIGHT;

    layer.buffer = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)frame_buffer1);
    layer.alphablend.src_alpha = 0xF4; /* src */
    layer.alphablend.dst_alpha = 0xF0; /* dst */
    layer.alphablend.src_alpha_op = display_alpha_op_override;
    layer.alphablend.dst_alpha_op = display_alpha_op_override;
    layer.background.u = 0xffff0000;
    layer.alphablend.mode = display_alphablend_mode_xor;

    if (status_success != lcdc_config_layer(LCD, layer_index, &layer, true)) {
        printf("failed to configure layer\n");
        while (1)
            ;
    }
}

void uvc2lcd_init(void)
{
    board_init_lcd();
    init_lcd();

    frame_pool[0].frame_buf = frame_buffer1;
    frame_pool[0].frame_bufsize = IMAGE_WIDTH * IMAGE_HEIGHT * 2;
    frame_pool[1].frame_buf = frame_buffer2;
    frame_pool[1].frame_bufsize = IMAGE_WIDTH * IMAGE_HEIGHT * 2;

    usbh_video_stream_init(5, frame_pool, 2);

    extern void usbh_video_fps_init(void);
    usbh_video_fps_init();
}