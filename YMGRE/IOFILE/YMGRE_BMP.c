#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMCS_File_IO.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"

// 文件信息头结构体
typedef struct {
    uint16 type; //必为'BM'
    uint32 size; //文件字节数(2-5)
    uint32 reserved;//位图文件保留字，必为0(6-9)
    uint32 off_bits; //像素数据偏移 (10-13)
}bmp_file_header_t;

//图像信息头结构体
typedef struct {
    uint32 size; // 结构体尺寸 (14-17)
    int32 width; // 图像宽度 (18-21)
    int32 height; // 图像高度 (22-25)
    uint16 planes; // 色彩平面数,目标设备的级别，为1(26-27)
    uint16 bit_count; // 像素位数，为1、4、8或16,24,32(28-29),为16,24,32时没有调色板(颜色表)
    uint32 compression; // 压缩方式，0为不压缩、1为BI_RLE8、2为BI_RLE4(30-33)
    uint32 size_image; // 单像素数据大小,等于bfSize-bfOffBits (34-37)
    int32 x_pels_permeter; // x方向分辨率，一般为0 (38-41)
    int32 y_pels_permeter; // y方向分辨率，一般为0 (42-45)
    uint32 clr_used; // 调色板颜色数，0表示使用所有调色板项(46-49)
    uint32 clr_important; // 重要颜色索引的数目，0表示都重要(50-53)
} bmp_info_header_t;

//32位图像素信息结构体
typedef struct {
    uint8 b; //蓝色分量 (0-255)
    uint8 g; //绿色分量 (0-255)
    uint8 r; //红色分量 (0-255)
    uint8 alpha;// 保留，必须为0
} pixel32_info_t;

//24位图像素信息结构体
typedef struct {
    uint8 b; //蓝色分量 (0-255)
    uint8 g; //绿色分量 (0-255)
    uint8 r; //红色分量 (0-255)
} pixel24_info_t;

typedef struct {
    bmp_info_header_t bmiHeader;// 位图信息头
    //pixel_info_t bmiColors[1];//颜色表
} bmp_info_t;

static bmp_file_header_t s_bmp_file_header = { 0x4d42, 0, 0, 0};
static bmp_info_header_t s_bmp_info_header = { 0, 0, 0, 1, 8, 0, 0, 0, 0, 0, 0 };
// 实现参考 https://www.jb51.net/article/226189.htm

// 文件转 image
void YMGRE_Bmp_File_LoadTo_Image(const char* file_path, GRErgb24** image, uint16* col, uint16* row)
{
    YMGRE_FILE* file = NULL;
    uint32 line_width = 0;
    uint32 width = 0;
    uint32 height = 0;

    file = YMGRE_fopen(file_path, "rb");
    gre_log_explain((NULL == file), GRE_LOG_FILE, "文件打开失败");

    //读取文件头
    YMGRE_fread(&s_bmp_file_header.type, sizeof(uint16), 1, file);//由于4字节对齐，单独读取2字节
    YMGRE_fread(&s_bmp_file_header.size, sizeof(s_bmp_file_header) - 4, 1, file);//由于4字节对齐，所以要少读取4字节
    gre_log_explain(s_bmp_file_header.type != 0x4d42, GRE_LOG_FILE, "文件不是BMP格式");

    //读取信息头
    YMGRE_fread(&s_bmp_info_header, sizeof(s_bmp_info_header), 1, file);
    width = s_bmp_info_header.width;
    height = s_bmp_info_header.height;
    gre_log_explain((width == 0) || (width > 65535) || (height == 0) || (height > 65535), GRE_LOG_FILE, "BMP图像尺寸超出范围");
    gre_log_explain(s_bmp_info_header.compression != 0, GRE_LOG_FILE, "暂不支持带压缩的BMP");

    //输出
    *col = (uint16)width;
    *row = (uint16)height;
    *image = (GRErgb24*)GRE_ImageBuff_Malloc(width * height * sizeof(GRErgb24));
    //申请失败
    gre_log_explain(*image == NULL, GRE_LOG_Mem1, "图片内存申请失败");
    GRErgb24* buf = (*image);
    //需要调色板处理
    if (s_bmp_info_header.bit_count < 16)
    {
        //申请失败
        gre_log_explain(1, GRE_LOG_FILE, "暂不支持带调色板图像（像素bit数<16）");
        //char temp[4 * 256] = { 0 };//调色板
        //fread(temp, 4 * 256, 1, file);
        //分别为 2*4 ，16*4 ，256*4个字节的调色板
    }
    //无需调色板
    else
    {
        if (s_bmp_info_header.bit_count == 24)
        {
            //BMP每行字节数不是4的倍数，则以0补齐
            line_width = (width * sizeof(pixel24_info_t) + 3) / 4 * 4;
            uint8* linebuff = GRE_malloc0(line_width);
            for (int i = height - 1; i >= 0; i--)
            {
                YMGRE_fread(linebuff, 1, line_width, file);
                GRErgb24* bufo = &buf[i * width];
                //有效部分输出
                for (int j = 0; j < width; j++)
                {
                    pixel24_info_t* pixel = (pixel24_info_t*)&linebuff[j * sizeof(pixel24_info_t)];
                    //记录
                    bufo[j].R = pixel->r;
                    bufo[j].G = pixel->g;
                    bufo[j].B = pixel->b;
                }
            }
            GRE_free0(linebuff);
        }
        else
        {
            gre_log_explain(s_bmp_info_header.bit_count == 16, GRE_LOG_FILE, "该像素为16位，待添加解码");
            gre_log_explain(s_bmp_info_header.bit_count == 32, GRE_LOG_FILE, "该像素为32位，待添加解码");
        }
    }

    YMGRE_fclose(file);
}

//image 转 文件
void YMGRE_Image_LoadTo_Bmp_File(const char* file_path, GRErgb24* image, uint32 width, uint32 height)
{
    YMGRE_FILE* file = NULL;
    gre_log_explain((NULL == image), GRE_LOG_PtrI, "输入图像不存在");

    uint32 line_width = (width * sizeof(pixel24_info_t) + 3) / 4 * 4;
    s_bmp_file_header.type = 0x4d42;//'BM'
    //带调色板
    uint16 clr_nums = 0;//4 * 256
    s_bmp_file_header.off_bits = (sizeof(bmp_file_header_t) - 2) + sizeof(bmp_info_header_t) + clr_nums;//由于4字节对齐，所以要少2个字节
    //文件大小
    s_bmp_file_header.size = s_bmp_file_header.off_bits + line_width * height;

    //信息头
    s_bmp_info_header.size = sizeof(bmp_info_header_t);
    s_bmp_info_header.width = width;
    s_bmp_info_header.height = height;
    s_bmp_info_header.size_image = line_width * height;
    s_bmp_info_header.clr_used = 0;//不使用调色板
    s_bmp_info_header.clr_important = 0;
    s_bmp_info_header.compression = 0;//不带压缩
    s_bmp_info_header.planes = 1;
    s_bmp_info_header.bit_count = 24;//24位
    s_bmp_info_header.x_pels_permeter = 1;
    s_bmp_info_header.y_pels_permeter = 1;

    file = YMGRE_fopen(file_path, "wb");
    gre_log_explain((NULL == file), GRE_LOG_FILE, "文件打开失败");

    //必须分开写入，由于未能4字节对齐
    YMGRE_fwrite(&s_bmp_file_header.type, sizeof(s_bmp_file_header.type), 1, file);//先写入2字节
    YMGRE_fwrite(&s_bmp_file_header.size, sizeof(s_bmp_file_header) - 4, 1, file);//剩余字节写入，因为4字节对齐所以分开写入
    //直接写入信息块
    YMGRE_fwrite(&s_bmp_info_header, sizeof(bmp_info_header_t), 1, file);

    ////写入调色板
    //uint8_t alpha = 0;
    //for (i = 0; i < 256; i++)
    //{
    //    fwrite(&i, 1, sizeof(uint8_t), file);
    //    fwrite(&i, 1, sizeof(uint8_t), file);
    //    fwrite(&i, 1, sizeof(uint8_t), file);
    //    fwrite(&alpha, 1, sizeof(uint8_t), file);
    //}
    uint8* linebuff = GRE_malloc0(line_width);
    GRE_memset(linebuff, 0, line_width);//初始化

    for (int32 i = height - 1; i >= 0; i--)
    {
        GRErgb24* bufo = &image[i * width];
        //有效部分写入
        for (int j = 0; j < width; j++)
        {
            pixel24_info_t* pixel = (pixel24_info_t*)&linebuff[j * sizeof(pixel24_info_t)];
            //记录
            pixel->b = bufo[j].B;
            pixel->g = bufo[j].G;
            pixel->r = bufo[j].R;
        }
        YMGRE_fwrite(linebuff, 1, line_width, file);//写入24bit
    }
    GRE_free0(linebuff);
    //fflush(file);//刷入内存
    //关闭文件自动刷入内存中
    YMGRE_fclose(file);
}
