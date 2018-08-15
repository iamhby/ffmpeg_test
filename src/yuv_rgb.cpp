
#include "main.h"

#if(HBY_RUN==1)

typedef struct RGB24 {
    _uint8    rgbBlue;      // 蓝色分量
    _uint8    rgbGreen;     // 绿色分量
    _uint8    rgbRed;       // 红色分量
    // _uint8    rgbReserved;  // 保留字节（用作Alpha通道或忽略）
} RGB24;

void itoa(int num, char* buf);
void rgb24_to_yuv420(const _uint8 *yuvBuffer_in, const _uint8 *rgbBuffer_out, int width, int height);
void yuv420_to_rgb24(const _uint8 *yuvBuffer_in, const _uint8 *rgbBuffer_out, int width, int height);
int simplest_rgb24_to_bmp(const char *rgb24path, int width, int height, const char *bmppath);
int rgb_to_bmp(unsigned char* pdata, const char *pFileName, int width, int height);

int main()
{
    char name[20] = { 0 };
    itoa(6552, name);
    printf("%s\n", name);


    FILE *fp_yuv;
    fopen_s(&fp_yuv, "sintel_640_360.yuv", "rb");

    FILE *fp_rgb;
    fopen_s(&fp_rgb, "out.rgb24", "wb");

    FILE *fp_yuv_out;
    fopen_s(&fp_yuv_out, "out.yuv", "wb");

    int width = 640;
    int height = 360;

    int yuvSize = width * height * 3 / 2;
    int rgbSize = width * height * sizeof(RGB24);

    _uint8 *yuvBuffer = (_uint8 *)malloc(yuvSize);
    _uint8 *rgbBuffer = (_uint8 *)malloc(rgbSize);


    for (int i = 0;; i++)
    {
        int readedsize = fread(yuvBuffer, 1, yuvSize, fp_yuv);
        if (feof(fp_yuv))
            break;

        yuv420_to_rgb24(yuvBuffer, rgbBuffer, width, height);

        fwrite(rgbBuffer, 1, rgbSize, fp_rgb);

#if 1
        fclose(fp_rgb);
        char name[256] = { 0 };
        sprintf(name, "bmp/output_lena%04d.bmp", i);
        simplest_rgb24_to_bmp("out.rgb24", 640, 360, name);

        sprintf(name, "bmp/output_lena%04d_.bmp", i);
        rgb_to_bmp(rgbBuffer, name, 640, 360);
        break;
#else

        memset(yuvBuffer, 0, yuvSize);
        rgb24_to_yuv420(rgbBuffer, yuvBuffer, width, height);

        fwrite(yuvBuffer, 1, yuvSize, fp_yuv_out);
#endif


    }
    fclose(fp_yuv_out);
    fclose(fp_yuv);
    fclose(fp_rgb);
    return 0;
}


void itoa(int num, char* buf)
{
    int tem = num;
    int len = 1;
    while (tem / 10)
    {
        len++;
        tem /= 10;
    }

    buf[len--] = '\0';
    while (num > 0)
    {
        buf[len--] = '0' + num % 10;
        num /= 10;
    }
}




void yuv420_to_rgb24(const _uint8 *yuvBuffer_in, const _uint8 *rgbBuffer_out, int width, int height)
{
    _uint8 *yuvBuffer = (_uint8 *)yuvBuffer_in;
    RGB24 *rgb24Buffer = (RGB24 *)rgbBuffer_out;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int index = y * width + x;

            int indexY = y * width + x;
            int indexU = width * height + y / 2 * width / 2 + x / 2;
            int indexV = width * height + width* height / 4 + y / 2 * width / 2 + x / 2;

            _uint8 Y = yuvBuffer[indexY];
            _uint8 U = yuvBuffer[indexU];
            _uint8 V = yuvBuffer[indexV];

            RGB24 *rgbNode = &rgb24Buffer[index];

            rgbNode->rgbRed = Y + 1.402 * (V - 128);
            rgbNode->rgbGreen = Y - 0.34413 * (U - 128) - 0.71414*(V - 128);
            rgbNode->rgbBlue = Y + 1.772*(U - 128);
        }
    }
}


void rgb24_to_yuv420(const _uint8 *rgbBuffer_in, const _uint8 *yuvBuffer_out, int width, int height)
{
    RGB24 *rgb24Buffer = (RGB24 *)rgbBuffer_in;

    _uint8 *py = (_uint8 *)yuvBuffer_out;;
    _uint8 *pu = py + width*height;
    _uint8 *pv = pu + width *height / 4;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int index = y * width + x;

            RGB24 *rgbNode = &rgb24Buffer[index];

            _uint8 indexY = 0.299*rgbNode->rgbRed + 0.587*rgbNode->rgbGreen + 0.114*rgbNode->rgbBlue;
            _uint8 indexU = -0.1687*rgbNode->rgbRed - 0.3313*rgbNode->rgbGreen + 0.5*rgbNode->rgbBlue + 128;
            _uint8 indexV = 0.5*rgbNode->rgbRed - 0.4187*rgbNode->rgbGreen - 0.0813*rgbNode->rgbBlue + 128;


            *(py++) = indexY;
            if (y % 2 == 0 && x % 2 == 0)
            {
                *(pu++) = indexU;
                //else if (x % 2 == 0)
                *(pv++) = indexV;
            }
        }
    }
}



int simplest_rgb24_to_bmp(const char *rgb24path, int width, int height, const char *bmppath)
{
    typedef struct
    {
        long imageSize;
        long blank;
        long startPosition;
    }BmpHead;

    typedef struct
    {
        long  Length;
        long  width;
        long  height;
        unsigned short  colorPlane;
        unsigned short  bitColor;
        long  zipFormat;
        long  realSize;
        long  xPels;
        long  yPels;
        long  colorUse;
        long  colorImportant;
    }InfoHead;

    int i = 0, j = 0;
    BmpHead m_BMPHeader = { 0 };
    InfoHead  m_BMPInfoHeader = { 0 };
    char bfType[2] = { 'B', 'M' };
    int header_size = sizeof(bfType)+sizeof(BmpHead)+sizeof(InfoHead);
    unsigned char *rgb24_buffer = NULL;
    FILE *fp_rgb24 = NULL, *fp_bmp = NULL;

    if (fopen_s(&fp_rgb24, rgb24path, "rb") != NULL){
        printf("Error: Cannot open input RGB24 file.\n");
        return -1;
    }
    if (fopen_s(&fp_bmp, bmppath, "wb") != NULL){
        printf("Error: Cannot open output BMP file.\n");
        return -1;
    }

    rgb24_buffer = (unsigned char *)malloc(width*height * 3);
    fread(rgb24_buffer, 1, width*height * 3, fp_rgb24);

    m_BMPHeader.imageSize = 3 * width*height + header_size;
    m_BMPHeader.startPosition = header_size;

    m_BMPInfoHeader.Length = sizeof(InfoHead);
    m_BMPInfoHeader.width = width;
    //BMP storage pixel data in opposite direction of Y-axis (from bottom to top).
    m_BMPInfoHeader.height = -height;
    m_BMPInfoHeader.colorPlane = 1;
    m_BMPInfoHeader.bitColor = 24;
    m_BMPInfoHeader.realSize = 3 * width*height;

    fwrite(bfType, 1, sizeof(bfType), fp_bmp);
    fwrite(&m_BMPHeader, 1, sizeof(m_BMPHeader), fp_bmp);
    fwrite(&m_BMPInfoHeader, 1, sizeof(m_BMPInfoHeader), fp_bmp);

    //BMP save R1|G1|B1,R2|G2|B2 as B1|G1|R1,B2|G2|R2
    //It saves pixel data in Little Endian
    //So we change 'R' and 'B'
    for (j = 0; j < height; j++){
        for (i = 0; i < width; i++){
            char temp = rgb24_buffer[(j*width + i) * 3 + 2];
            rgb24_buffer[(j*width + i) * 3 + 2] = rgb24_buffer[(j*width + i) * 3 + 0];
            rgb24_buffer[(j*width + i) * 3 + 0] = temp;
        }
    }
    fwrite(rgb24_buffer, 3 * width*height, 1, fp_bmp);
    fclose(fp_rgb24);
    fclose(fp_bmp);
    free(rgb24_buffer);
    printf("Finish generate %s!\n", bmppath);
    return 0;
}


typedef long LONG;
typedef unsigned long DWORD;
typedef unsigned short WORD;

typedef struct {
    //WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BMPFILEHEADER_T;

typedef struct{
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
} BMPINFOHEADER_T;

int rgb_to_bmp(unsigned char* pdata, const char *pFileName, int width, int height)
{
    int ret = 0;
    FILE *bmp_fd = NULL;

    //分别为rgb数据，要保存的bmp文件名
    int size = width*height * 3 * sizeof(char); // 每个像素点3个字节
    // 位图第一部分，文件信息
    BMPFILEHEADER_T bfh;

    fopen_s(&bmp_fd, pFileName, "wb");

    if (NULL == bmp_fd)
    {
        ret = -3;
        return ret;
    }

    //bfh.bfType = (unsigned short)0x4d42;  //bm
    unsigned short bfType = 0x4d42;
    bfh.bfSize = size  // data size
        + sizeof(BMPFILEHEADER_T) // first section size
        + sizeof(BMPINFOHEADER_T) // second section size
        ;
    printf("sizeof( BMPFILEHEADER_T )== %ld,sizeof( BMPINFOHEADER_T )=%ld\n", sizeof(BMPFILEHEADER_T), sizeof(BMPINFOHEADER_T));
    bfh.bfReserved1 = 0; // reserved
    bfh.bfReserved2 = 0; // reserved
    bfh.bfOffBits = sizeof(BMPFILEHEADER_T)+sizeof(BMPINFOHEADER_T);//真正的数据的位置
    printf("bmp_head== %ld\n", bfh.bfOffBits);
    // 位图第二部分，数据信息
    BMPINFOHEADER_T bih;
    bih.biSize = sizeof(BMPINFOHEADER_T);
    bih.biWidth = width;
    bih.biHeight = -height;//BMP图片从最后一个点开始扫描，显示时图片是倒着的，所以用-height，这样图片就正了
    bih.biPlanes = 1;//为1，不用改
    bih.biBitCount = 24;
    bih.biCompression = 0;//不压缩
    bih.biSizeImage = size;

    bih.biXPelsPerMeter = 0;//像素每米

    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;//已用过的颜色，为0,与bitcount相同
    bih.biClrImportant = 0;//每个像素都重要

    fwrite(&bfType, sizeof(bfType), 1, bmp_fd);
    fwrite(&bfh, 6, 1, bmp_fd);
    fwrite(&bfh.bfReserved2, sizeof(bfh.bfReserved2), 1, bmp_fd);
    fwrite(&bfh.bfOffBits, sizeof(bfh.bfOffBits), 1, bmp_fd);
    fwrite(&bih, sizeof(BMPINFOHEADER_T), 1, bmp_fd);

    fwrite(pdata, size, 1, bmp_fd);
    if (NULL != bmp_fd)
    {
        fclose(bmp_fd);
        bmp_fd = NULL;
    }
    return ret;
}

#endif
