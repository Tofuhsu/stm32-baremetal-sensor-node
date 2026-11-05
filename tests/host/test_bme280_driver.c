/* Runs the ACTUAL firmware C driver against the emulator bus/peripheral models.
 * This verifies MMIO protocol logic, not ARM instructions or Renode behavior.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "periph/bme280.h"
#include "../../src/temperature.h"

static bus_t *test_bus;
static uint32_t test_primask;
uint32_t firmware_mmio_read(uint32_t address) { return bus_read32(test_bus,address); }
void firmware_mmio_write(uint32_t address,uint32_t value) { bus_write32(test_bus,address,value); }
uint32_t irq_save(void) { uint32_t previous=test_primask; test_primask=1; return previous; }
void irq_restore(uint32_t mask) { test_primask=mask&1u; }
#include "../../src/bme280.c"

static int64_t floor_shift(int64_t value, unsigned bits)
{
    int64_t unit=INT64_C(1)<<bits;
    return value/unit-(value<0 && value%unit!=0);
}

static void test_general_compensation(void)
{
    const int32_t calibration[][3]={{21000,23000,-700},{30000,25000,1200},{24000,25600,0}};
    unsigned accepted=0;
    for(unsigned c=0;c<3;++c) for(uint32_t raw=0;raw<=0xFFFFFu;raw+=197u) {
        int64_t d1=(int64_t)(raw>>3)-2*calibration[c][0];
        int64_t d2=(int64_t)(raw>>4)-calibration[c][0];
        int64_t fine=floor_shift(d1*calibration[c][1],11)+floor_shift(floor_shift(d2*d2,12)*calibration[c][2],14);
        int64_t expected=floor_shift(fine*5+128,8);
        int32_t result=123456;
        bool ok=sensor_compensate_temperature(raw,(uint32_t)calibration[c][0],calibration[c][1],calibration[c][2],&result);
        bool valid=expected>=-4000 && expected<=8500 && raw!=0x80000u;
        assert(ok==valid);
        if(ok) { assert(result==expected); ++accepted; }
        else assert(result==123456);
    }
    assert(accepted>1000);
    for(int32_t value=-4000;value<=8500;++value) {
        char actual[8],expected[16]; unsigned magnitude=(unsigned)(value<0?-value:value);
        snprintf(expected,sizeof expected,"%s%u.%02u",value<0?"-":"",magnitude/100,magnitude%100);
        assert(sensor_format_temperature(value,actual) && !strcmp(actual,expected));
    }
    puts("PASS checked int32 compensation: independent int64 oracle with nonzero T3; every centidegree format in supported range");
}

int main(void)
{
    test_general_compensation();
    test_bus=bus_new(); rcc_t *rcc=rcc_new();
    gpio_t *gpio=gpio_new("GPIOB",GPIOB_BASE,rcc,RCC_GPIOB_ENABLE_BIT);
    i2c_t *i2c=i2c_new(rcc,gpio); bme280_t *bme=bme280_new(0x76);
    assert(test_bus && rcc && gpio && i2c && bme);
    assert(!bus_add_periph(test_bus,rcc_as_periph(rcc)));
    assert(!bus_add_periph(test_bus,gpio_as_periph(gpio)));
    assert(!bus_add_periph(test_bus,i2c_as_periph(i2c)));
    assert(!i2c_attach(i2c,bme280_as_device(bme)));
    assert(bme280_init() && !test_primask);
    assert(bus_read32(test_bus,I2C1_BASE+I2C_CCR)==80 && bus_read32(test_bus,I2C1_BASE+I2C_TRISE)==17);
    assert(gpio_pin_is_af(gpio,8,4) && gpio_pin_is_af(gpio,9,4));
    uint8_t two[2]; assert(read_registers(0x88,two,2));
    assert(((unsigned)two[0]|((unsigned)two[1]<<8))==24000);
    const int32_t values[]={2500,3050,-1234,8500,-4000};
    for(unsigned mask=0;mask<2;++mask) {
        test_primask=mask;
        for(unsigned j=0;j<sizeof values/sizeof values[0];++j) {
            assert(bme280_set_temperature(bme,values[j])); int32_t value=0;
            assert(bme280_read_temperature(&value) && value==values[j]);
            assert(test_primask==mask);
        }
    }
    const struct {int32_t value; const char *text;} formats[]={
        {2500,"25.00"},{3050,"30.50"},{-1234,"-12.34"},{0,"0.00"},{-1,"-0.01"},{8500,"85.00"},{-4000,"-40.00"}};
    for(unsigned i=0;i<sizeof formats/sizeof formats[0];++i) {
        char text[8]; assert(sensor_format_temperature(formats[i].value,text));
        assert(!strcmp(text,formats[i].text));
    }
    int32_t untouched=123; assert(!sensor_compensate_temperature(0x80000,24000,25600,0,&untouched));
    assert(!sensor_compensate_temperature(0xFFFFF,0,32767,32767,&untouched) && untouched==123);
    i2c_stats_t st=i2c_get_stats(i2c);
    assert(!st.protocol_errors && !st.bad_accesses && !st.pin_mismatch_starts && !st.pe_disabled_transfers);
    assert(!bus_fault(test_bus));
    /* Missing device must return an error, not wait forever or consume bogus calibration. */
    bus_free(test_bus); i2c_free(i2c); bme280_free(bme); gpio_free(gpio); rcc_free(rcc);
    test_bus=bus_new(); rcc=rcc_new(); gpio=gpio_new("GPIOB",GPIOB_BASE,rcc,RCC_GPIOB_ENABLE_BIT); i2c=i2c_new(rcc,gpio);
    assert(!bus_add_periph(test_bus,rcc_as_periph(rcc)) && !bus_add_periph(test_bus,gpio_as_periph(gpio)) && !bus_add_periph(test_bus,i2c_as_periph(i2c)));
    assert(!bme280_init());
    bus_free(test_bus); i2c_free(i2c); gpio_free(gpio); rcc_free(rcc);
    puts("PASS actual firmware C driver: I2C 1/2/N byte paths, init, T=25.00/30.50/-12.34/endpoints, PRIMASK restore, NACK recovery");
}
