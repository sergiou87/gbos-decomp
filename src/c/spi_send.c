typedef unsigned char u8;

__at(0xbe00) volatile u8 spi_data;

void spi_send(u8 value)
{
    spi_data = value;
}
