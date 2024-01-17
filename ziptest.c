/*
 * ZipTest  Version 1.1 2024-01-17
 * -------------------------------
 *
 * Utility to test the installed ZIP memory on an Amiga 3000 motherboard.
 * This program only runs correctly on the Amiga 3000 and will malfunction
 * (and likely crash) on other Amiga models.
 *
 * Copyright 2024 Chris Hooper.  This program and source may be used
 * and distributed freely, for any purpose which benefits the Amiga
 * community.  Commercial use of the binary, source, or algorithms requires
 * prior written or email approval from Chris Hooper <amiga@cdh.eebugs.com>.
 * All redistributions must retain this Copyright notice.
 *
 * DISCLAIMER: THE SOFTWARE IS PROVIDED "AS-IS", WITHOUT ANY WARRANTY.
 * THE AUTHOR ASSUMES NO LIABILITY FOR ANY DAMAGE ARISING OUT OF THE USE
 * OR MISUSE OF THIS UTILITY OR INFORMATION REPORTED BY THIS UTILITY.
 */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <proto/dos.h>
#include <clib/exec_protos.h>
#include <clib/timer_protos.h>

const char *version = "\0$VER: ZIPTest 1.1 (2024-01-17) by Chris Hooper";

extern struct ExecBase *SysBase;

/*
 * Doing the call-out for a cache line flush has been measured as being
 * slower than just turning the cache off before doing the test.  If you
 * enable this macro, it disables the cache flush.  Not all code in this
 * utility supports this and instead just turns off the data cache around
 * the critical section.
 */
#undef USE_CACHE_LINE_FLUSH

/*
 * Enable code which tests the conversion to and from an Amiga physical
 * address from a RAS+CAS address.  Only needed to verify this program's
 * conversion routines are correct.  When enabled, the program will do
 * nothing other than verify the conversion code.
 */
#undef TEST_BANK_AMASK_TO_ADDRESS

/*
 * Memory cell test block size -- note that FS-UAE only emulates a
 * single 4k block of "fake" memory.
 */
#define TESTBLOCK_SIZE 4096


/*
 * Quick rundown of known ZIP parts which work in the Amiga 3000.  These
 * were taken from various places, but most from the Amiga User Manual.
 *
 * ========================================================================
 * 1Mx4bit Static Column ZIP chips
 * -------------------------------
 * Hitachi: HM514402BZ6 HM514402__8
 * OKI: MSM514402 M514402A-__Z M514402B-__Z
 *      __ is one of 60, 70, or 80 -- designates speed grade (60, 70, 80 ns)
 * Toshiba: TC514402Z-80 TC514402AZ-70_
 * NEC: D424402V-70
 *
 * ========================================================================
 * 1Mx4bit ZIP chips supporting Fast Page Mode
 * -------------------------------------------
 * Fujitsu: MB814400A-70PSZ
 * Hitachi: HM514400AZ HM514400ALZ HM514400ASLZ
 * Micron: MT4C4001JCZ
 * Mitsubishi: M5M44400AL
 * Motorola (Freescale): MCM514400Z
 * NEC: uPD424400V
 * OKI: MSM514400-__ZS MSM514400-_ZS MSM514400-_AZS MSM514400A__ZS
 *      MSM514400AL__ZS
 *      __ is one of 60, 70, or 80 -- designates speed grade (60, 70, 80 ns)
 *      _  is one of 6, 7, or 8 -- designates speed grade (60, 70, 80 ns)
 * Samsung: KM44C1000AZ KM44C1000ASLZ KM44C1000ALZ KM44C1000BLZ KM44C1000BSLZ
 *          KM44C1000BZ KM44C1000CLZ KM44C1000CSLZ KM44C1000CZ KM44C1000LZ
 *          KM44C1000Z
 * Toshiba: TC514400Z TC514400AZ TC514400AZL TC514400AAZ TC514400AAZL
 *
 * ========================================================================
 * 256Kx4bit Static Column ZIP chips
 * ---------------------------------
 * Hitachi: HM514258
 * Mitsubishi: M5M44258BL-8
 * NEC: uPD424258
 * OKI: MSM514258
 * Sharp: LH64258
 * TI: TMS44C258
 *
 * =======================================================================
 * 256Kx4bit ZIP chips supporting Fast Page Mode
 * ---------------------------------------------
 * Fujitsu: MB81C4256A-70PSZ
 * Hitachi: HM514256
 * NEC: uPD424256
 * OKI: MSM514256
 * Toshiba: TC514256
 * Samsung: KM44C256DZ
 *
 * ========================================================================
 */

/*
 * Probed addresses to RAS/CAS A9-A0 on Amiga 1Mx4 DRAM.
 *            RAS:A9-A0  CAS:A9-A0
 *            ---------- ----------
 * 0x07c00000 1111111110 1111111110
 * 0x07c00004 1111111110 1111111111
 * 0x07c00008 1111111110 1111111100
 * 0x07c00010 1111111110 1111111010
 * 0x07c00020 1111111110 1111110110
 * 0x07c00040 1111111110 1111101110
 * 0x07c00080 1111111110 1111011110
 * 0x07c00100 1111111110 1110111110
 * 0x07c00200 1111111110 1101111110
 * 0x07c00400 1111111110 1011111110
 * 0x07c00800 1111111110 0111111110
 * 0x07c01000 1111111100 1111111110
 * 0x07c02000 1111111010 1111111110
 * 0x07c04000 1111110110 1111111110
 * 0x07c08000 1111101110 1111111110
 * 0x07c10000 1111011110 1111111110
 * 0x07c20000 1110111110 1111111110
 * 0x07c40000 1101111110 1111111110
 * 0x07c80000 1011111110 1111111110
 * 0x07d00000 0111111110 1111111110
 * 0x07e00000 1111111111 1111111110
 *
 * Probed addresses to RAS/CAS A8-A0 pins on Amiga 256Kx4 DRAM.
 *            RAS:A8-A0 CAS:A8-A0
 *            --------- ---------
 * 0x07f00000 111111110 111111110
 * 0x07f00004 111111110 111111111
 * 0x07f00008 111111110 111111100
 * 0x07f00010 111111110 111111010
 * 0x07f00020 111111110 111110110
 * 0x07f00040 111111110 111101110
 * 0x07f00080 111111110 111011110
 * 0x07f00100 111111110 110111110
 * 0x07f00200 111111110 101111110
 * 0x07f00400 111111110 011111110
 * 0x07f00800 111111111 111111110
 * 0x07f01000 111111101 111111110
 * 0x07f02000 111111010 111111110
 * 0x07f04000 111110110 111111110
 * 0x07f08000 111101110 111111110
 * 0x07f10000 111011110 111111110
 * 0x07f20000 110111110 111111110
 * 0x07f40000 101111110 111111110
 * 0x07f80000 011111110 111111110
 *
 * [ Note that A0 is inverted in all DIP and ZIP accesses]
 *
 *
 * From the Ramsey specification, the following address ranges
 * map to the RAS lines with Amiga 1mx4 DRAM (RSIZE=1, RAMWIDTH=1)
 *
 * 0x07000000 RAS0
 * 0x07400000 RAS1
 * 0x07800000 RAS2
 * 0x07c00000 RAS3
 *
 * Amiga 246x4 DRAM (RSIZE=0, RAMWIDTH=1)
 * 0x07c00000 RAS0
 * 0x07d00000 RAS1
 * 0x07e00000 RAS2
 * 0x07f00000 RAS3
 */

#define ADDR8(x)      (volatile uint8_t *)(x)
#define ADDR16(x)     (volatile uint16_t *)(x)
#define ADDR32(x)     (volatile uint32_t *)(x)

#define ARRAY_SIZE(x) ((sizeof (x) / sizeof ((x)[0])))
#define BIT(x)        (1 << (x))

// #define AMIGA_CHIP_RAM_ADDR   0x00010000  /* Read address to force bus access */
#define FASTMEM_TOP           0x08000000  /* Last fast memory address + 1 */
#define ZIP_BANKS             4           /* Number of fast memory banks */

#define CIAA_TIMB_LO          0x00bfe601
#define CIAA_TIMB_HI          0x00bfe701
#define CIAB_TOD_LO           0x00bfd800
#define CIAB_TOD_MID          0x00bfd900
#define CIAB_TOD_HI           0x00bfda00

#define RAMSEY_CONTROL        0x00de0003  /* Ramsey control register */
#define RAMSEY_VERSION        0x00de0043  /* Ramsey version register */
#define AGNUS_DMACON_R        0x00dff002  /* Agnus DMA control register (R) */
#define AGNUS_DMACON_W        0x00dff096  /* Agnus DMA control register (W) */

#define RAMSEY_CONTROL_PAGE     (1 << 0)  /* 1=Page mode enabled */
#define RAMSEY_CONTROL_BURST    (1 << 1)  /* 1=Burst mode enabled */
#define RAMSEY_CONTROL_WRAP     (1 << 2)  /* 1=wrap, 0=no backward bursts */
#define RAMSEY_CONTROL_RAMSIZE  (1 << 3)  /* 1=1Mx4 (4MB), 0=256x4 (1MB)*/
#define RAMSEY_CONTROL_RAMWIDTH (1 << 4)  /* Ramsey-4 1=4-bit, 0=1=bit */
#define RAMSEY_CONTROL_SKIP     (1 << 4)  /* Ramsey-7 1=4-clocks, 0=5 clocks */
#define RAMSEY_CONTROL_REFRESH0 (1 << 5)  /* 00=154, 01=238, 10=380, 11=Off */
#define RAMSEY_CONTROL_REFRESH1 (1 << 6)  /* 00=154, 01=238, 10=380, 11=Off */
#define RAMSEY_CONTROL_TEST     (1 << 7)  /* 1=Test mode */

#define AMIGA_PPORT_DIR       0x00bfe301  /* Amiga parallel port dir register */
#define AMIGA_PPORT_DATA      0x00bfe101  /* Amiga parallel port data reg. */

#define FLAG_DEBUG            0x01        /* Debug output */
#define FLAG_MORE_DEBUG       0x02        /* More debug output */
#define FLAG_LONG_TEST        0x04        /* Perform more thorough tests */
#define FLAG_SHOW_DIP         0x08        /* Show DIP RAM positions */
#define FLAG_SHOW_MAP         0x10        /* Show data bus bits (don't test) */

#define POS_LEFT              0           /* ZIP IC in the left column */
#define POS_RIGHT             1           /* ZIP IC in the right column */
#define POS_BOTTOM            2           /* Only used for DIP ICs */

#define SC_MODE_NONE          0           /* Page off, burst off */
#define SC_MODE_BURST         1           /* Burst mode (68040: + Page mode) */
#define SC_MODE_PAGE          2           /* Page mode */
#define SC_MODE_BOTH          3           /* Burst mode and Page mode */

/* Ramsey-07 requires the processor to be in Supervisor state */
#define USE_SUPERVISOR_STATE
#ifdef USE_SUPERVISOR_STATE
#define SUPERVISOR_STATE_ENTER() { \
                                   APTR old_stack = SuperState()
#define SUPERVISOR_STATE_EXIT()    UserState(old_stack); \
                                 }
#else
#define SUPERVISOR_STATE_ENTER()
#define SUPERVISOR_STATE_EXIT()
#endif

/* Either prevent interrupts or prevent multitasking */
#define INTERRUPTS_DISABLE() Disable()  /* Disable Interrupts */
#define INTERRUPTS_ENABLE()  Enable()   /* Enable Interrupts */

#define CACHE_LINE_FLUSH(addr, len) CacheClearE((void *)(addr), len, \
                                                CACRF_ClearD)
#define CACHE_ENABLE_DATA() \
        { \
            uint32_t oldcachestate = \
            CacheControl(CACRF_EnableD, CACRF_EnableD) & \
                         (CACRF_EnableD | CACRF_DBE);
#define CACHE_DISABLE_DATA() \
        { \
            uint32_t oldcachestate = \
            CacheControl(0L, CACRF_EnableD) & (CACRF_EnableD | CACRF_DBE)
#define CACHE_ENABLE_BURST() \
            oldcachestate |= (CacheControl(CACRF_DBE, CACRF_DBE) & CACRF_DBE)
#define CACHE_DISABLE_BURST() \
            oldcachestate |= (CacheControl(0, CACRF_DBE) & CACRF_DBE)
#define CACHE_RESTORE_STATE() \
            CacheControl(oldcachestate, CACRF_EnableD | CACRF_DBE); \
        }

/* MMU_DISABLE() and MMU_RESTORE() must be called from Supervisor state */
#define MMU_DISABLE() \
        { \
            uint32_t oldmmustate; \
            if (cpu_type == 68030) { \
                oldmmustate = mmu_get_tc_030(); \
                mmu_set_tc_030(oldmmustate & ~BIT(31)); \
            } else if ((cpu_type == 68040) || (cpu_type == 68060)) { \
                oldmmustate = mmu_get_tc_040(); \
                mmu_set_tc_040(oldmmustate & ~BIT(15)); \
            }
#define MMU_RESTORE() \
            if (cpu_type == 68030) { \
                mmu_set_tc_030(oldmmustate); \
            } else if ((cpu_type == 68040) || (cpu_type == 68060)) { \
                mmu_set_tc_040(oldmmustate); \
            } \
        }

/* Modern stdint types */
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

#ifndef __GNUC__
typedef unsigned int   uint;
#endif

void cpu_dcache_flush(void);
void burst_copyline(volatile void *dst, volatile void *src);
uint burst_copy(volatile void *dst, volatile void *src, uint len);
void burst_read_moveml(volatile void *src, uint size); // must not exceed 8MB
void burst_read_readl(volatile void *src, uint size);  // must not exceed 2MB
void burst_test_read(volatile void *dst, volatile void *src, uint flags);
uint32_t mmu_get_type(void);
uint32_t mmu_get_tc_030(void);
uint32_t mmu_get_tc_040(void);
void     mmu_set_tc_030(uint32_t tc);
void     mmu_set_tc_040(uint32_t tc);
uint16_t get_sr(void);
uint16_t irq_disable(void);
uint16_t irq_enable(void);
static uint cpu_type = 0;
static uint8_t cpu_can_do_burst = 0;
static uint8_t mmu_is_active = 0;
static uint8_t ramsey_version = 0;
static uint8_t ramsey_rev = 0;

static int get_mem_types(uint addrbits, uint32_t *bank_results, uint flags);

typedef struct {
    const char skt[6];    /* String description of socket, e.g. "U881" */
    uint8_t    bank;      /* Memory controller bank (0-3) */
    uint8_t    nibble;    /* Nibble within 32-bit word */
    uint8_t    position;  /* Left or right column of ZIP ICs */
    uint8_t    pins[4];   /* I/O pins IO1, IO2, IO3, IO4 to bits */
} u_to_bit_t;

/*
 * The locations of ZIP chips to banks of memory is not straight-forward.
 *
 * Observe the following table, which is laid out in the order of ZIP
 * sockets on the board (top is toward the rear connectors on the board):
 *
 * Socket Bank.Nibble     Socket Bank.Nibble
 *   U881    3.7            U879    3.5
 *   U873    2.7            U871    2.5
 *   U865    1.7            U863    1.5
 *   U857    0.7            U855    0.5
 *   U880    3.6            U878    3.4
 *   U872    2.6            U870    2.4
 *   U864    1.6            U862    1.4
 *   U856    0.6            U854    0.4
 *   U877    3.3            U875    3.1
 *   U869    2.3            U867    2.1
 *   U861    1.3            U859    1.1
 *   U853    0.3            U851    0.1
 *   U876    3.2            U874    3.0
 *   U868    2.2            U866    2.0
 *   U860    1.2            U858    1.0
 *   U852    0.2            U850    0.0
 *
 * The Bank is a particular 1MB or 4MB chunk of memory.
 * The Nibble is a nibble number within a particular 32-bit value
 * within the address range of the bank.
 *
 * For a Bank to be recognized and usable by the Amiga, all 8 ZIP
 * chips of a bank must be populated (e.g. 0.0, 0.1, 0.2, 0.3, 0.4,
 * 0.5, 0.6, 0.7).  Note that some Amiga 3000 memory guides number
 * the bank and nibble starting with 1 instead of 0.
 */
static const u_to_bit_t zip_u_data[] = {
    { "U881",  3, 7, POS_LEFT,   {28, 29, 30, 31} },
    { "U879",  3, 5, POS_RIGHT,  {20, 21, 22, 23} },
    { "U873",  2, 7, POS_LEFT,   {28, 29, 30, 31} },
    { "U871",  2, 5, POS_RIGHT,  {20, 21, 22, 23} },
    { "U865",  1, 7, POS_LEFT,   {28, 29, 30, 31} },
    { "U863",  1, 5, POS_RIGHT,  {20, 21, 22, 23} },
    { "U857",  0, 7, POS_LEFT,   {28, 29, 30, 31} },
    { "U855",  0, 5, POS_RIGHT,  {20, 21, 22, 23} },

    { "U880",  3, 6, POS_LEFT,   {24, 25, 26, 27} },
    { "U878",  3, 4, POS_RIGHT,  {16, 17, 18, 19} },
    { "U872",  2, 6, POS_LEFT,   {24, 25, 26, 27} },
    { "U870",  2, 4, POS_RIGHT,  {16, 17, 18, 19} },
    { "U864",  1, 6, POS_LEFT,   {24, 25, 26, 27} },
    { "U862",  1, 4, POS_RIGHT,  {16, 17, 18, 19} },
    { "U856",  0, 6, POS_LEFT,   {24, 25, 26, 27} },
    { "U854",  0, 4, POS_RIGHT,  {16, 17, 18, 19} },

    { "U877",  3, 3, POS_LEFT,   {12, 13, 14, 15} },
    { "U875",  3, 1, POS_RIGHT,  { 4,  5,  6,  7} },
    { "U869",  2, 3, POS_LEFT,   {12, 13, 14, 15} },
    { "U867",  2, 1, POS_RIGHT,  { 4,  5,  6,  7} },
    { "U861",  1, 3, POS_LEFT,   {12, 13, 14, 15} },
    { "U859",  1, 1, POS_RIGHT,  { 4,  5,  6,  7} },
    { "U853",  0, 3, POS_LEFT,   {12, 13, 14, 15} },
    { "U851",  0, 1, POS_RIGHT,  { 4,  5,  6,  7} },

    { "U876",  3, 2, POS_LEFT,   { 8,  9, 10, 11} },
    { "U874",  3, 0, POS_RIGHT,  { 0,  1,  2,  3} },
    { "U868",  2, 2, POS_LEFT,   { 8,  9, 10, 11} },
    { "U866",  2, 0, POS_RIGHT,  { 0,  1,  2,  3} },
    { "U860",  1, 2, POS_LEFT,   { 8,  9, 10, 11} },
    { "U858",  1, 0, POS_RIGHT,  { 0,  1,  2,  3} },
    { "U852",  0, 2, POS_LEFT,   { 8,  9, 10, 11} },
    { "U850",  0, 0, POS_RIGHT,  { 0,  1,  2,  3} },
};

/* DIP chips */
static const u_to_bit_t dip_u_data[] = {
    { "U857D", 0, 7, POS_BOTTOM, {28, 29, 30, 31} },
    { "U856D", 0, 6, POS_BOTTOM, {24, 25, 27, 26} },
    { "U855D", 0, 5, POS_BOTTOM, {20, 21, 23, 22} },
    { "U854D", 0, 4, POS_BOTTOM, {16, 17, 19, 18} },
    { "U853D", 0, 3, POS_BOTTOM, {12, 13, 15, 14} },
    { "U852D", 0, 2, POS_BOTTOM, { 8,  9, 11, 10} },
    { "U851D", 0, 1, POS_BOTTOM, { 4,  5,  7,  6} },
    { "U850D", 0, 0, POS_BOTTOM, { 0,  1,  3,  2} },
};


/*
 * Ramsey refresh timing table, indexed by bit values from
 * RAMSEY_CONTROL_REFRESH0 and RAMSEY_CONTROL_REFRESH1.
 *
 * Ramsey documentation seems to be incorrect as to the number of
 * clock cycles which Ramsey uses depending on the value of the
 * refresh bits in the Ramsey control register.
 *
 *                            --Documented--  --Measured--
 *       Documented Measured    16M     25M     16M    25M
 * Index Clocks     Clocks     usec    usec    usec   usec
 *   0   154        156        9.24    6.16    9.72   6.24
 *   1   238        240       14.28    9.52   15.00   9.60
 *   2   380        372       22.80   15.20   23.25  14.88
 *   3   Infinite   Infinite      -       -       -      -
 *
 * The position of the 16M/25M did selects Index 0 or Index 1 at reset,
 * but the number of measured clocks at a given Index did not change
 * regardless of the position of the 16M/25M jumper.
 */
static const struct {
    char clocks[4];
    char interval_16m[12];  /* 16.67 MHz */
    char interval_25m[12];  /* 25.00 MHz */
} ramsey_refresh_timing[] = {
    { "156", "9.72 usec",  "6.24 usec"  },  /* RATE 00 */
    { "240", "15.00 usec", "9.60 usec"  },  /* RATE 01 */
    { "372", "23.25 usec", "14.88 usec" },  /* RATE 10 */
    { "N/A", "No refresh", "No refresh" },  /* RATE 11 */
};


/*
 * print_binary() - display binary representation of value
 */
static void
print_bits(uint count, uint32_t value)
{
    while (count-- > 0)
        printf("%u", (value >> count) & 1);
    printf("\n");
}

#if defined(__GNUC__)
#define tolower(x) ((x) | 0x20)
#endif

#if defined(__VBCC__) || defined(__GNUC__)
#define stricmp(x, y) strcasecmp(x, y)

/* strcasecmp() - simple string compare which ignores uppercase vs lowercase */
int
strcasecmp(const char *str1, const char *str2)
{
    while (*str1 != '\0') {
        if (tolower(*str1) != tolower(*str2))
            break;
        str1++;
        str2++;
    }
    return (tolower(*str1) - tolower(*str2));
}
#endif


/*
 * amask_to_address() - convert a mask of RAS + CAS bits to an Amiga CPU
 *                      physical memory address.  CAS are always the lower
 *                      bits.  There is not a simple mapping from RAS + CAS
 *                      bits presented on the wire to Amiga CPU physical
 *                      memory addresses.  It differs depending on whether
 *                      J852 has the Ramsey in 256Kx4 mode or 1Mx4 mode.
 *                      This function assumes x4 memory (and does not support
 *                      x1 memory).
 */
static uint32_t
amask_to_address(uint bank, uint32_t amask, uint addrbits)
{
    uint32_t bank_size = BIT(addrbits) * 4;  /* Assumes 4-bit wide ZIP ICs */
    uint32_t addr      = amask;

#undef DEBUG_AMASK
#ifdef DEBUG_AMASK
    printf(" original amask: ");
    print_bits(addrbits, amask);
#endif
    if (addrbits == 20) {
        /*
         * For 1Mx4 DRAM:
         * 1) Invert bits 1-9 and bits 11-19
         * 2) Roll bit 10 to bit 19 and shuffle bits 19-11 right one bit
         */
        amask ^= 0xffbfe;
        amask = ((amask & BIT(10)) << 9) |              /* Bit 10 << 9 */
                ((amask & (BIT(20) - BIT(11))) >> 1) |  /* Bits 11-19 >> 1 */
                (amask & (BIT(10) - 1));                /* Bits 0-9 */
    } else {
        /* For 256Kx4 DRAM:
         * 1) Invert bits 1-8 and bits 10-17
         */
        amask ^= 0x3fdfe;
    }
    /* Left shift the mask by 2 bits, since 8x 4-bit devices are in parallel */
    amask <<= 2;

    addr = FASTMEM_TOP - bank_size * (bank + 1) + amask;
#ifdef DEBUG_AMASK
    printf("converted amask: ");
    print_bits(addrbits, amask);
    printf(" memory address: %08x\n", addr);
#endif
    return (addr);
}

#ifdef TEST_BANK_AMASK_TO_ADDRESS
/*
 * address_to_amask() - convert an Amiga physical memory address to a mask
 *                      of RAS + CAS bits.  CAS are always the lower bits.
 */
static uint32_t
address_to_amask(uint32_t addr, uint addrbits)
{
    uint32_t amask = addr;

    /* Right shift address by 2 bits, since 8x 4-bit devices are in parallel */
    amask >>= 2;

    if (addrbits == 20) {
        /*
         * For 1Mx4 DRAM:
         * 1) Mask off bits 20 and above
         * 2) Roll bit 19 to bit 10 and shuffle bits 18-10 left by one
         * 3) Invert bits 1-9 and bits 11-19
         */
        amask &= (BIT(20) - 1);
        amask = ((amask & BIT(19)) >> 9) |              /* Bit 19 >> 9 */
                ((amask & (BIT(19) - BIT(10))) << 1) |  /* Bits 10-18 << 1 */
                (amask & (BIT(10) - 1));                /* Bits 0-9 */
        amask ^= 0xffbfe;
    } else {
        /*
         * For 1Mx4 DRAM:
         * 1) Mask off bits 18 and above
         * 2) Invert bits 1-8 and bits 10-17
         */
        amask &= (BIT(18) - 1);
        amask ^= 0x3fdfe;
    }
#ifdef DEBUG_AMASK
    printf("  back to amask: ");
    print_bits(addrbits, amask);
    printf("\n");
#endif
    return (amask);
}

/* Addresses corresponding to walking 0 and 1 on address lines for 1Mx4 mode */
static const uint32_t test_1mx4[] = {
    0xffbfe, 0xffbff, 0xffbfc, 0xffbfa, 0xffbf6, 0xffbee,
    0xffbde, 0xffbbe, 0xffb7e, 0xffafe, 0xff9fe, 0xff3fe,
    0xfebfe, 0xfdbfe, 0xfbbfe, 0xf7bfe, 0xefbfe, 0xdfbfe,
    0xbfbfe, 0x7fbfe, 0xffffe,
    0x00001, 0x00002, 0x00004, 0x00008, 0x00010, 0x00020,
    0x00040, 0x00080, 0x00100, 0x00200, 0x00400,
};

/* Addresses corresponding to walking 0 and 1 on address lines for 256x4 mode */
static const uint32_t test_256kx4[] = {
    0x3fdfe, 0x3fdff, 0x3fdfc, 0x3fdfa, 0x3fdf6, 0x3fdee,
    0x3fdde, 0x3fdbe, 0x3fd7e, 0x3fcfe, 0x3fffe, 0x3fbfe,
    0x3f5fe, 0x3edfe, 0x3ddfe, 0x3bdfe, 0x37dfe, 0x2fdfe,
    0x1fdfe,
    0x00001, 0x00002, 0x00004, 0x00008, 0x00010, 0x00020,
    0x00040, 0x00080, 0x00100, 0x00200,
};

static uint
selftest_check_conv(uint32_t value, uint mem_addrbits)
{
    uint32_t addr  = amask_to_address(0, value, mem_addrbits);
    uint32_t amask = address_to_amask(addr, mem_addrbits);
    if (value != amask) {
        printf("ERROR - conversion failed\n"
               "  original=%06x ", value);
        print_bits(mem_addrbits, value);
        printf("  reverted=%06x ", amask);
        print_bits(mem_addrbits, amask);
        printf("      addr=%08x\n", addr);
        return (1);
    }
    return (0);
}

/*
 * selftest_bank_amask_to_address() tests program code which can convert
 *                                  to and from a RAS/CAS memory address
 */
static void
selftest_bank_amask_to_address(void)
{
    uint i;
    uint errs = 0;

    /* Check 20-bit conversions (1Mx4) */
    for (i = 0; i < ARRAY_SIZE(test_1mx4); i++)
        errs += selftest_check_conv(test_1mx4[i], 20);

    /* Check 18-bit conversions (256Kx4) */
    for (i = 0; i < ARRAY_SIZE(test_256kx4); i++)
        errs += selftest_check_conv(test_256kx4[i], 18);

    if (errs == 0)
        printf("No conversion errors detected\n");
}
#endif

/*
 * get_status() - return bit test status, one of Good, 0 (Stuck low),
 *                1 (Stuck high), or ? (Floats)
 */
const char *
get_status(uint32_t bitvals, uint32_t result_or, uint32_t result_and,
           uint32_t result_diff)
{
    const char *status;

    if ((result_or & bitvals) == 0)
        status = "0";   /* Stuck 0 */
    else if (result_and & bitvals)
        status = "1";   /* Stuck 1 */
    else if (result_diff & bitvals)
        status = "!";   /* Floats */
    else
        status = "Good";
    return (status);
}

/*
 * usage() - display program usage
 */
static void
usage(void)
{
    printf("This tool will perform simple tests on ZIP memory installed in\n"
           "an Amiga 3000 motherboard.  Options:\n"
           "    ADDR   - perform address line test\n"
           "    ASCII  - show ASCII ART of chip positions and pins\n"
           "    CELL   - perform memory cell test (verify every bit)\n"
           "    DATA   - perform data line test\n"
           "    DIP    - show DIP RAM positions\n"
           "    DEBUG  - enable debug output\n"
           "    INFO   - only show system information\n"
           "    FORCE  - ignore fact enforcer is present\n"
           "    LONG   - perform more thorough (slower) line test\n"
           "    MAP    - just show map of corresponding bits (no test)\n"
           "    QUIET  - do not display banner\n"
           "    SPROBE - probe for static-column memory (68030 only)\n"
           "    STROBE - generate power-of-two address strobes for a probe\n");
}

/*
 * show_ascii_art() - display map of ZIP and DIP sockets and pinout of each
 */
static void
show_ascii_art(void)
{
    uint pos;
    printf("Amiga 3000 Fastmem ZIP memory sockets (back to front)\n");
    for (pos = 0; pos < ARRAY_SIZE(zip_u_data); pos++) {
        printf("   %s %u.%u", zip_u_data[pos].skt, zip_u_data[pos].bank,
               zip_u_data[pos].nibble);
        if (zip_u_data[pos].position == POS_RIGHT)
            printf("\n");
    }
    printf("\n"
           "Fastmem DIP memory sockets (left to right)\n"
           "   U857D U856D U854D U853D U852D U851D U850D\n"
           "\n"
           "   DIP   __   __        ZIP          W\n"
           "        |  \\_/  |                    R  N\n"
           "    IO1-|1    20|-VSS          I  I  I  .\n"
           "    IO2-|2    19|-IO4       C  O  O  T  C  A  A  A  A  A\n"
           "  WRITE-|3    18|-IO3       S  4  1  E  .  1  3  4  6  8\n"
           "    RAS-|4    17|-CS        |  |  |  |  |  |  |  |  |  |  (back)\n"
           "     A9-|5    16|-OE      ______________________________\n"
           "     A0-|6    15|-A8     /  2  4  6  8 10 12 14 16 18 20\\\n"
           "     A1-|7    14|-A7    (                                )\n"
           "     A2-|8    13|-A6     \\______________________________/\n"
           "     A3-|9    12|-A5     1 |  |  |  |  |  |  |  |  |  |\n"
           "    VCC-|10   11|-A4       O  I  V  I  R  A  A  V  A  A   (face)\n"
           "        |_______|          E  O  S  O  A  0  2  C  5  7\n"
           "                              3  S  2  S        C\n");
}

/* show_dip_header() - display header text identifying the DIP packages */
static void
show_dip_header(void)
{
    uint pos;
    printf("\n      ");
    for (pos = 0; pos < ARRAY_SIZE(dip_u_data); pos++)
        printf("%-6s", dip_u_data[pos].skt);
    printf("\n      ");
    for (pos = 0; pos < ARRAY_SIZE(dip_u_data); pos++)
        printf("%u.%-4u", dip_u_data[pos].bank, dip_u_data[pos].nibble);
    printf("\n     ");
    for (pos = 0; pos < ARRAY_SIZE(dip_u_data); pos++)
        printf(" -----");
    printf("\n");
}

/* enforcer_check() - verify enforcer is not running */
static int
enforcer_check(void)
{
    Forbid();
    if (FindTask("« Enforcer »") != NULL) {
        /* Enforcer is running */
        Permit();
        printf("Enforcer is present.  First use \"enforcer off\" to "
               "disable enforcer.\n");
        return (1);
    }
    if (FindTask("« MuForce »") != NULL) {
        /* MuForce is running */
        Permit();
        printf("MuForce is present.  First use \"muforce off\" to "
               "disable MuForce.\n");
        return (1);
    }
    Permit();
    return (0);
}

static uint8_t
get_ramsey_version(void)
{
    uint8_t version;
    SUPERVISOR_STATE_ENTER();
    version = *ADDR8(RAMSEY_VERSION);
    SUPERVISOR_STATE_EXIT();
    return (version);
}

static uint8_t
get_ramsey_control(void)
{
    uint8_t control;
    SUPERVISOR_STATE_ENTER();
    control = *ADDR8(RAMSEY_CONTROL);
    SUPERVISOR_STATE_EXIT();
    return (control);
}

static void
set_ramsey_control(uint32_t control)
{
    uint timeout;
    uint8_t got;
    SUPERVISOR_STATE_ENTER();
    *ADDR8(RAMSEY_CONTROL) = control;
    for (timeout = 1 << 16; timeout > 0; timeout--) {
        got = *ADDR8(RAMSEY_CONTROL);
        if (((got ^ control) &
            (RAMSEY_CONTROL_PAGE | RAMSEY_CONTROL_BURST |
             RAMSEY_CONTROL_WRAP | RAMSEY_CONTROL_SKIP)) == 0) {
            break;
        }
    }
    SUPERVISOR_STATE_EXIT();

    if (timeout == 0) {
        printf("Ramsey timeout %02x != expected %02x\n", got, control);
        return;
    }
}

const uint
get_ramsey_clock_OLD(void)
{
    /* Count how many Ramsey register writes can be done in a single tick */
    uint     iters;
    uint     mhz;
    uint     access_per_tick;
    uint     ediff;
    uint     ediffmax;
    ULONG    freq;
    struct EClockVal eclk_start;
    struct EClockVal eclk_end;

    return (0);
    SUPERVISOR_STATE_ENTER();
    Forbid();
    freq = ReadEClock(&eclk_start);  // Interrupts required by ReadEClock()
    ediffmax = freq / 10;
    iters = 0;
    while (1) {
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        *ADDR8(RAMSEY_VERSION) = 0;
        if ((++iters & 0x00003fff) == 0) {
            if ((iters & 0x0001ffff) == 0)
                break;  /* Timeout */

            ReadEClock(&eclk_end);
            ediff = eclk_end.ev_lo - eclk_start.ev_lo;
            if (ediff > 71590) // 100ms
                break;
        }
    }

    Permit();
    SUPERVISOR_STATE_EXIT();
    access_per_tick = iters * 16 * freq / ediff;
#define DCC_O2_MAGIC_FACTOR 2809
    mhz = (access_per_tick * DCC_O2_MAGIC_FACTOR + 500000) / 1000000;
    printf("iters=%u ediff=%u freq=%u access/tick=%u %u\n",
           iters, ediff, freq, access_per_tick, access_per_tick * DCC_O2_MAGIC_FACTOR);

    return (mhz);
}

const uint
get_cpu(void)
{
    UWORD attnflags = SysBase->AttnFlags;

    if (attnflags & 0x80)
        return (68060);
    if (attnflags & AFF_68040)
        return (68040);
    if (attnflags & AFF_68030)
        return (68030);
    if (attnflags & AFF_68020)
        return (68020);
    if (attnflags & AFF_68010)
        return (68010);
    return (68000);
}

/*
 * test_value() writes a value to memory and reads it back, returning the
 *              result.  In order to avert bus capacitance causing false
 *              "good" values, a disturb value to different memory is
 *              emitted before the written value is read back from memory.
 */
static uint32_t
test_value(uint32_t addr, uint32_t value)
{
    uint32_t orig_zip;
    uint32_t flip_zip;
    uint32_t flip_addr;
    uint32_t result;

    flip_addr = addr ^ 0x0400000;  // Choose another fastmem bank

    orig_zip           = *ADDR32(addr);
    flip_zip           = *ADDR32(flip_addr);
    *ADDR32(addr)      = value;
    CACHE_LINE_FLUSH(ADDR32(addr), 16);
    *ADDR32(flip_addr) = ~value;
    CACHE_LINE_FLUSH(ADDR32(flip_addr), 16);
    result             = *ADDR32(addr);
    *ADDR32(addr)      = orig_zip;
    *ADDR32(flip_addr) = flip_zip;

    return (result);
}

/*
 * Perform a simple data line test on the memory by walking through a set
 * of patterns designed to expose bad components or solder joints.
 * Address lines are left at 0 for this test, so if they are stuck high
 * or stuck low it should not matter.  If they are floating, then bad data
 * lines might be reported here.
 */
static uint32_t
test_dbits(uint32_t addr, uint bitvals, uint32_t *bits_and, uint32_t *bits_or,
           uint flags)
{
    uint32_t result_and  = 0xffffffff;
    uint32_t result_or   = 0;
    uint32_t result_diff = 0;
    uint32_t result;
    uint32_t test_seq[6];
    uint     pos;
    uint     count;
    uint     passes = (flags & FLAG_LONG_TEST) ? 2048 : 512;

    /* Push out previous output */
    fflush(stdout);

    test_seq[0] = 0x00000000;  /* Test stuck high */
    test_seq[1] = 0xffffffff;  /* Test stuck low */
    test_seq[2] = bitvals;     /* Test only interesting bits high (floating) */
    test_seq[3] = ~bitvals;    /* Test only interesting bits low (floating) */
    test_seq[4] = 0xa5a5a5a5;  /* Set every other bit (floating) */
    test_seq[5] = 0x5a5a5a5a;  /* Set every other bit (floating) */

    CACHE_DISABLE_DATA();
    SUPERVISOR_STATE_ENTER();
    INTERRUPTS_DISABLE();
    MMU_DISABLE();
    for (count = 0; count < passes; count++) {
        for (pos = 0; pos < ARRAY_SIZE(test_seq); pos++) {
            result = test_value(addr, test_seq[pos]);
            result_and  &= result;
            result_or   |= result;
            result_diff |= (result ^ test_seq[pos]);
        }
    }
    MMU_RESTORE();
    INTERRUPTS_ENABLE();
    SUPERVISOR_STATE_EXIT();
    CACHE_RESTORE_STATE();

    *bits_and = result_and;
    *bits_or  = result_or;
    return (result_diff);
}

/*
 * data_line_test() - test data lines connected to ZIP memory packages
 *
 * This test will hold the address lines constant while walking patterns
 * on the data lines (IO1-IO4) of the Amiga ZIP memory.  It will then
 * report address lines per ZIP package as Good, stuck high (1), stuck
 * low (0), or floating (?).
 */
static int
data_line_test(uint addrbits, uint flags)
{
    size_t   pos;
    int      errs = 0;
    uint     io_pin;
    uint32_t bitvals;
    uint32_t result_and  = 0;
    uint32_t result_or   = 0;
    uint32_t result_diff = 0;
    const char *socket_l1 = "Socket   IO1  IO2  IO3  IO4 ";
    const char *socket_l2 = "-------- ---- ---- ---- ----";

    /*
     * Note that the data line test currently uses 76 columns for its
     * output. This is the maximum that the default Amiga console font
     * can handle without having overscan (such is the default when
     * no Startup-Sequence is used, or we boot from floppy).
     */
    if (flags & FLAG_DEBUG) {
        socket_l1 = "Socket   ADDR    IO1  IO2  IO3  IO4 ";
        socket_l2 = "-------- ------- ---- ---- ---- ----";
    }
    printf("Data line %s\n", (flags & FLAG_SHOW_MAP) ? "map" : "test");
    printf("  %s  %s\n"
           "  %s  %s\n", socket_l1, socket_l1, socket_l2, socket_l2);

    for (pos = 0; pos < ARRAY_SIZE(zip_u_data); pos++) {
        uint     bank   = zip_u_data[pos].bank;
        uint     nibble = zip_u_data[pos].nibble;
        uint32_t addr   = amask_to_address(bank, 0, addrbits);

        printf("  %s %u.%u", zip_u_data[pos].skt, zip_u_data[pos].bank, nibble);
        if (flags & FLAG_DEBUG)
            printf(" %07x", addr + nibble / 2);
        if (flags & FLAG_SHOW_MAP) {
            for (io_pin = 0; io_pin < 4; io_pin++)
                printf("  %2u ", zip_u_data[pos].pins[io_pin]);
        } else {
            bitvals = 0;
            if ((flags & FLAG_SHOW_DIP) && (zip_u_data[pos].bank == 0)) {
                for (io_pin = 0; io_pin < 4; io_pin++)
                    printf(" ----");
            } else if (flags & FLAG_LONG_TEST) {
                for (io_pin = 0; io_pin < 4; io_pin++) {
                    bitvals = BIT(zip_u_data[pos].pins[io_pin]);

                    result_diff = test_dbits(addr, bitvals, &result_and,
                                             &result_or, flags);
                    if (result_diff != 0)
                        errs++;
                    printf(" %-4s", get_status(bitvals, result_or, result_and,
                                               result_diff));
                }
            } else {
                for (io_pin = 0; io_pin < 4; io_pin++)
                    bitvals |= BIT(zip_u_data[pos].pins[io_pin]);

                result_diff = test_dbits(addr, bitvals, &result_and,
                                         &result_or, flags);
                if (result_diff != 0)
                    errs++;

                for (io_pin = 0; io_pin < 4; io_pin++) {
                    bitvals = BIT(zip_u_data[pos].pins[io_pin]);
                    printf(" %-4s", get_status(bitvals, result_or, result_and,
                                               result_diff));
                }
            }
        }
        if (zip_u_data[pos].position == POS_RIGHT)
            printf("\n");
    }
    if (flags & FLAG_SHOW_DIP) {
        uint     bank = dip_u_data[0].bank;
        uint32_t addr = amask_to_address(bank, 0, addrbits);

        show_dip_header();
        if (!(flags & (FLAG_LONG_TEST | FLAG_SHOW_MAP))) {
            /* Test all 32 bits at once */
            bitvals = 0xffffffff;
            result_diff = test_dbits(addr, bitvals, &result_and, &result_or,
                                     flags);
            if (result_diff != 0)
                errs++;
        }

        for (io_pin = 0; io_pin < 4; io_pin++) {
            printf("  IO%u ", io_pin + 1);
            for (pos = 0; pos < ARRAY_SIZE(dip_u_data); pos++) {
                bitvals = BIT(dip_u_data[pos].pins[io_pin]);
                if (flags & FLAG_SHOW_MAP) {
                    printf("%-6u", dip_u_data[pos].pins[io_pin]);
                    continue;
                }
                if (flags & FLAG_LONG_TEST) {
                    /* Test one bit at a time */
                    result_diff = test_dbits(addr, bitvals, &result_and,
                                             &result_or, flags);
                    if (result_diff != 0)
                        errs++;
                }
                printf("%-6s", get_status(bitvals, result_or, result_and,
                                          result_diff));
            }
            printf("\n");
        }
    }
    return (errs);
}

static const char aaaa[]    = " A A A A A A A A A A";
static const char a9a0[]    = " 9 8 7 6 5 4 3 2 1 0";
static const char a9a0w[]   = " A9 A8 A7 A6 A5 A4 A3 A2 A1 A0";
static const char dashes[]  = " - - - - - - - - - -";
static const char dashesw[] = " -- -- -- -- -- -- -- -- -- --";

/* Display a map of addresses corresponding to single address lines */
static void
address_line_map(uint addrbits)
{
    uint bit;
    uint abit;
    uint casbits = addrbits / 2;
    static const char uscore[] = "________";

    printf("\nAddress line map\n");
    printf("  Bank 0   %sRAS%s  %sCAS%s\n"
           "  Address %s %s\n"
           "  ------- %s %s\n",
           uscore, uscore, uscore, uscore, a9a0, a9a0, dashes, dashes);
    for (bit = 0; bit < addrbits; bit++) {
        uint32_t amask = BIT(bit);

        printf("  %07x", amask_to_address(0, amask, addrbits));
        for (abit = addrbits; abit > 0; abit--) {
            if ((abit == casbits) || (abit == addrbits)) {
                printf(" ");
                if (casbits == 9) {
                    printf("-");
                    continue;
                }
            }
            printf("%2d", (amask & BIT(abit - 1)) ? 1 : 0);
        }
        printf("\n");
    }
}

/*
 * address_line_test() - test address lines connected to ZIP memory packages
 *
 * General algorithm:
 * For each bank of DRAM, and for each power-of-two (RAS=CAS) address
 * 1) Store data from possible bit flip addresses above and below bit (B)
 *    under test and write a unique pattern to each (11111111, 22222222, ...)
 *    xx000xx, xx001xx, xx010xx, xx011xx, xx100xx, xx101xx, xx110xx, xx111xx.
 *    If B is 0, then the "below" bit becomes the high address bit.
 *    If B is the high address bit, then the "above" becomes bit 0.
 * 2) Capture all current values and restore original values.
 * 3) Analyze nibbles of all captured values against expected value.
 * 4) Do the above for both other bits x=0 and other bits x=1 (walking
 *    ones and walking zeros).
 */
static int
address_line_test(uint addrbits, uint flags)
{
    uint     bank;
    uint     casbit;
    uint     cur;
    uint     pos;
    uint     walk_zero_one;
    uint     walk_count    = 2;
    uint     bad_threshold = 16;
    uint     casbits       = addrbits / 2;
    int      errs          = 0;
    int      show_type     = 0;
    uint32_t bank_results[ZIP_BANKS];
    uint32_t save_addrs[8];
    uint32_t save_data[8];
    uint16_t cas_bit_badcount[ZIP_BANKS][10][8];  /* [bank][rascas][nibbles] */

    if (flags & FLAG_LONG_TEST) {
        walk_count = 16;
        bad_threshold = 256;
    }

    printf("Address line test\n");
    memset(cas_bit_badcount, 0, sizeof (cas_bit_badcount));

    for (bank = 0; bank < ZIP_BANKS; bank++) {
        /* Walk other bits as both 000..000 and 111..111 */
        for (walk_zero_one = 0; walk_zero_one < walk_count; walk_zero_one++) {
            uint32_t otherbitmask = (BIT(casbits) - 1) * (walk_zero_one & 1);

            /*
             * Walk bit position [0..(CASBITS-1)].
             * At each position, walk three bits (000, 001, 010, 011, ...)
             * where the center bit is the specific bit position.  Positions
             * at the end roll over/under to the bit at the other end.
             */
            for (casbit = 0; casbit < casbits; casbit++) {
                uint32_t bitl = (casbit + casbits - 1) % casbits;  /* -1 */
                uint32_t bitm = casbit;
                uint32_t bith = (casbit + 1) % casbits;            /* +1 */
                uint32_t maskval = BIT(bitl) | BIT(bitm) | BIT(bith);

                uint threebit;
                for (threebit = 0; threebit < 8; threebit++) {
                    uint32_t orval       = BIT(bitl) * (threebit & 1) |
                                           BIT(bitm) * ((threebit >> 1) & 1) |
                                           BIT(bith) * ((threebit >> 2) & 1);
                    uint32_t cas_addr    = orval | otherbitmask & ~maskval;
                    uint32_t rascas_addr = cas_addr | (cas_addr << casbits);

                    save_addrs[threebit] = amask_to_address(bank, rascas_addr,
                                                            addrbits);
                    if (flags & FLAG_MORE_DEBUG) {
                        printf("%06x ", save_addrs[threebit]);
                        print_bits(20, rascas_addr);
                    }
                }

                CACHE_DISABLE_DATA();
                SUPERVISOR_STATE_ENTER();
                INTERRUPTS_DISABLE();
                MMU_DISABLE();

                /* Store data and pattern memory */
                for (cur = 0; cur < ARRAY_SIZE(save_addrs); cur++) {
                    save_data[cur] = *ADDR32(save_addrs[cur]);
                    *ADDR32(save_addrs[cur]) = 0x11111111 * (cur + 1);
                }
                /* Verify pattern and restore data */
                for (cur = 0; cur < ARRAY_SIZE(save_addrs); cur++) {
                    uint32_t temp = *ADDR32(save_addrs[cur]);
                    *ADDR32(save_addrs[cur]) = save_data[cur];
                    save_data[cur] = temp;
                }
                MMU_RESTORE();
                INTERRUPTS_ENABLE();
                SUPERVISOR_STATE_EXIT();
                CACHE_RESTORE_STATE();

                /* Verify pattern */
                if (flags & FLAG_MORE_DEBUG)
                    printf("CASbit=%u", casbit);
                for (cur = 0; cur < ARRAY_SIZE(save_addrs); cur++) {
                    if (flags & FLAG_MORE_DEBUG)
                        printf(" %08x:%08x:%08x", save_addrs[cur],
                               save_data[cur], 0x11111111 * (cur + 1));
                    if (save_data[cur] != 0x11111111 * (cur + 1)) {
                        /* Mismatch -- compare individual nibbles */
                        uint     nibble;
                        uint32_t temp = save_data[cur];
                        errs++;
                        for (nibble = 0; nibble < 8; nibble++) {
                            /*
                             * Compare each nibble against its expected value
                             * If there is a mismatch, then the value read
                             * XOR with the value written tells us which bit(s)
                             * are likely at fault.  This is not foolproof, but
                             * is a good hint.
                             *
                             * [Note: we do +1 when writing and -1 when reading
                             *        back so that we can use 0x00 and > 0x08
                             *        as invalid values.  The examples below
                             *        ignore this fact. ]
                             *
                             * Example (write before +1, and after -1 of read):
                             * Wrote: 0100  Read: 0101 at offset 4 (xx100xx)
                             *     The low address bit is suspect because
                             *     the 100 value was written to offset 4,
                             *     and the 101 value was written to offset 5.
                             *
                             * Example:
                             * Wrote: 0100  Read: 0010 at offset 4 (xx100xx)
                             *     The upper two address bits are suspect
                             *     because the 100 value was written to
                             *     offset 4, and the 010 value was written
                             *     to offset 2.
                             */
                            uint8_t v = temp & 0xf;
                            if ((v - 1) != cur) {
                                uint8_t xor = (uint8_t) ((v - 1) ^ cur);
                                if ((v == 0) || (v > 8))
                                    xor = 7;  /* Mark all bits bad */
                                if (xor & 1)
                                    cas_bit_badcount[bank][bitl][nibble]++;
                                if (xor & 2)
                                    cas_bit_badcount[bank][bitm][nibble]++;
                                if (xor & 4)
                                    cas_bit_badcount[bank][bith][nibble]++;
#ifdef DEBUG_ADDR_MISMATCH
                                if ((xor != 7) && (bank == 3))
                                    printf("%x[%x:%x]%06x:%x:%x.%x.%x ", xor,
                                           v - 1, cur, save_addrs[cur], bank,
                                           bith, bitm, bitl);
#endif
                            }
                            temp >>= 4;
                        }
                    }
                }
                if (flags & FLAG_MORE_DEBUG)
                    printf("\n");
            }
        }
    }

    if (get_mem_types(addrbits, bank_results, flags) == 0)
        show_type = 1;

    if (flags & FLAG_DEBUG) {
        if (show_type) {
            printf("  Socket  %s Type  Socket  %s Type\n"
                   "  --------%s ----  --------%s ----\n",
                   a9a0w, a9a0w, dashes, dashes);
        } else {
            printf("  Socket  %s  Socket  %s\n"
                   "  --------%s  --------%s\n",
                   a9a0w, a9a0w, dashesw, dashesw);
        }
    } else {
        if (show_type) {
            printf("%30s Mem %30s Mem\n", aaaa, aaaa);
            printf("  Socket  %s Type  Socket  %s Type\n"
                   "  --------%s ----  --------%s ----\n",
                   a9a0, a9a0, dashes, dashes);
        } else {
            printf("%30s%30s\n", aaaa, aaaa);
            printf("  Socket  %s  Socket  %s\n"
                   "  --------%s  --------%s\n", a9a0, a9a0, dashes, dashes);
        }
    }
    for (pos = 0; pos < ARRAY_SIZE(zip_u_data); pos++) {
        uint nibble = zip_u_data[pos].nibble;
        uint was_bad = 0;
        bank = zip_u_data[pos].bank;
        printf("  %s %u.%u", zip_u_data[pos].skt, zip_u_data[pos].bank, nibble);
        if (casbits < 10) {
            printf(" -");
            if (flags & FLAG_DEBUG)
                printf(" ");
        }
        for (casbit = casbits; casbit-- > 0; ) {
            uint badcount = cas_bit_badcount[bank][casbit][nibble];
            if (flags & FLAG_DEBUG)
                printf(" %2u", (badcount <= 99) ? badcount : 99);
            else if (badcount == 0)
                printf(" G");
            else if (badcount < bad_threshold)
                printf(" ?");
            else
                printf(" !");
            was_bad += badcount;
        }
        if (show_type) {
            printf(" %-4s", was_bad ? "?" :
                   (bank_results[bank] & BIT(nibble)) ? "SC" : "FPM");
        }
        if (zip_u_data[pos].position == POS_RIGHT)
            printf("\n");
    }
    if (flags & FLAG_SHOW_DIP) {
        show_dip_header();
        for (casbit = 0; casbit < casbits; casbit++) {
            uint lbank = dip_u_data[0].bank;

            printf("  A%u ", casbit);
            for (pos = 0; pos < ARRAY_SIZE(dip_u_data); pos++) {
                uint nibble   = dip_u_data[pos].nibble;
                uint badcount = cas_bit_badcount[lbank][casbit][nibble];
                if (flags & FLAG_DEBUG)
                    printf(" %5u", cas_bit_badcount[lbank][casbit][nibble]);
                else if (badcount == 0)
                    printf(" Good ");
                else if (badcount < 16)
                    printf(" ?    ");
                else
                    printf(" !    ");
            }
            printf("\n");
        }
    }
    return (errs);
}

static uint32_t
memory_read_usec(int sc_mode, uint xsize)
{
    uint8_t  ramsey_control_old;
    uint8_t  ramsey_control_new;
    uint     ediff;
    ULONG    freq;
    uint     calltime;
    uint32_t usec;
    struct EClockVal eclk_start;
    struct EClockVal eclk_end;

    ramsey_control_old = get_ramsey_control();
    switch (sc_mode) {
        case SC_MODE_NONE:
            ramsey_control_new = ramsey_control_old &
                                 ~(RAMSEY_CONTROL_BURST | RAMSEY_CONTROL_PAGE);
            break;
        case SC_MODE_BURST:
            ramsey_control_new = (ramsey_control_old & ~RAMSEY_CONTROL_PAGE) |
                                 RAMSEY_CONTROL_BURST;
            break;
        case SC_MODE_PAGE:
            ramsey_control_new = (ramsey_control_old & ~RAMSEY_CONTROL_BURST) |
                                 RAMSEY_CONTROL_PAGE;
            break;
        case SC_MODE_BOTH:
            ramsey_control_new = ramsey_control_old |
                                 RAMSEY_CONTROL_BURST | RAMSEY_CONTROL_PAGE;
            break;
    }
//  printf("next mode %u xs=%x\n", sc_mode, xsize);

    SUPERVISOR_STATE_ENTER();
    INTERRUPTS_DISABLE();
    ReadEClock(&eclk_start);
    ReadEClock(&eclk_start);
    ReadEClock(&eclk_end);
    INTERRUPTS_ENABLE();
    calltime = eclk_end.ev_lo - eclk_start.ev_lo;

    INTERRUPTS_DISABLE();
 // CacheClearU();            // Last chance for write-back
    cpu_dcache_flush();       // Last chance for write-back
    ReadEClock(&eclk_start);  // Interrupts required by ReadEClock()
    MMU_DISABLE();

    *ADDR8(RAMSEY_CONTROL) = ramsey_control_new;
    while (*ADDR8(RAMSEY_CONTROL) != ramsey_control_new)
        ;
    burst_read_readl(ADDR32(0x07c00000), xsize);

    *ADDR8(RAMSEY_CONTROL) = ramsey_control_old;
    while (*ADDR8(RAMSEY_CONTROL) != ramsey_control_old)
        ;

    MMU_RESTORE();
    cpu_dcache_flush();       // Ensure no corrupt data is retained
    freq = ReadEClock(&eclk_end);
    INTERRUPTS_ENABLE();
    SUPERVISOR_STATE_EXIT();

    ediff = eclk_end.ev_lo - eclk_start.ev_lo - calltime;
#if 0
    printf("mode=%x control old=%02x new=%02x ediff=%u\n",
           sc_mode, ramsey_control_old, ramsey_control_new, ediff);
#endif
    usec = (ediff * 10000 / (freq / 10)) * 10;
    if (usec == 0)
        usec = 10;

    return (usec);
}

/*
 * cpu_can_burst
 * -------------
 * Returns non-zero when the CPU can do burst reads
 */
static int
cpu_can_burst(void)
{
    uint32_t xsize = 1 << 18;  // 256 K  (must not exceed 4MB)
    uint32_t usec_off;
    uint32_t usec_burst;
    uint     pct_x_10;

    CACHE_ENABLE_DATA();
    CACHE_ENABLE_BURST();
    usec_off   = memory_read_usec(SC_MODE_NONE, xsize);
    usec_burst = memory_read_usec(SC_MODE_BURST, xsize);
    CACHE_RESTORE_STATE();

    pct_x_10   = usec_off * 1000 / usec_burst;
#if 0
    printf("usec_off=%u usec_burst=%u\n", usec_off, usec_burst);
#endif

    /* Consider anything greater than 4% as burst-capable */
    return (pct_x_10 > 1040);
}

static void
sc_memory_speed(void)
{
    int      iters = 1;
    uint32_t usec_off;
    uint32_t usec_burst;
    uint32_t usec_fpm;
    uint32_t usec_both;
    uint32_t xsize = 1 << 17;

    CACHE_ENABLE_DATA();
    CACHE_ENABLE_BURST();
    usec_off   = memory_read_usec(SC_MODE_NONE, xsize);
    usec_burst = memory_read_usec(SC_MODE_BURST, xsize);
    usec_fpm   = memory_read_usec(SC_MODE_PAGE, xsize);
    usec_both  = memory_read_usec(SC_MODE_BOTH, xsize);
    CACHE_RESTORE_STATE();

    /* Measure and report memory speed with static column mode on and off */
    printf("With datacache, with burst\n");
    printf("Off:   %u KB/sec\n", iters * xsize * 1000 / usec_off);
    printf("Burst: %u KB/sec\n", iters * xsize * 1000 / usec_burst);
    printf("Page:  %u KB/sec\n", iters * xsize * 1000 / usec_fpm);
    printf("Both:  %u KB/sec\n", iters * xsize * 1000 / usec_both);

    printf("\nWith datacache, no burst\n");
    CACHE_ENABLE_DATA();
    CACHE_DISABLE_BURST();
    usec_off   = memory_read_usec(SC_MODE_NONE, xsize);
    usec_burst = memory_read_usec(SC_MODE_BURST, xsize);
    usec_fpm   = memory_read_usec(SC_MODE_PAGE, xsize);
    usec_both  = memory_read_usec(SC_MODE_BOTH, xsize);
    CACHE_RESTORE_STATE();
    printf("Off:   %u KB/sec\n", iters * xsize * 1000 / usec_off);
    printf("Burst: %u KB/sec\n", iters * xsize * 1000 / usec_burst);
    printf("Page:  %u KB/sec\n", iters * xsize * 1000 / usec_fpm);
    printf("Both:  %u KB/sec\n", iters * xsize * 1000 / usec_both);

    printf("\nNo datacache, burst\n");
    CACHE_DISABLE_DATA();
    CACHE_ENABLE_BURST();
    usec_off   = memory_read_usec(SC_MODE_NONE, xsize);
    usec_burst = memory_read_usec(SC_MODE_BURST, xsize);
    usec_fpm   = memory_read_usec(SC_MODE_PAGE, xsize);
    usec_both  = memory_read_usec(SC_MODE_BOTH, xsize);
    CACHE_RESTORE_STATE();

    printf("Off:   %u KB/sec\n", iters * xsize * 1000 / usec_off);
    printf("Burst: %u KB/sec\n", iters * xsize * 1000 / usec_burst);
    printf("Page:  %u KB/sec\n", iters * xsize * 1000 / usec_fpm);
    printf("Both:  %u KB/sec\n", iters * xsize * 1000 / usec_both);
    printf("\nNo datacache, no burst\n");
    CACHE_DISABLE_DATA();
    CACHE_DISABLE_BURST();
    usec_off   = memory_read_usec(SC_MODE_NONE, xsize);
    usec_burst = memory_read_usec(SC_MODE_BURST, xsize);
    usec_fpm   = memory_read_usec(SC_MODE_PAGE, xsize);
    usec_both  = memory_read_usec(SC_MODE_BOTH, xsize);
    CACHE_RESTORE_STATE();
    printf("Off:   %u KB/sec\n", iters * xsize * 1000 / usec_off);
    printf("Burst: %u KB/sec\n", iters * xsize * 1000 / usec_burst);
    printf("Page:  %u KB/sec\n", iters * xsize * 1000 / usec_fpm);
    printf("Both:  %u KB/sec\n", iters * xsize * 1000 / usec_both);
}

#define CIAA_TBHI 0x00bfe701
#define CIAA_TBLO 0x00bfe601
static uint cia_ticks(void);

#define RAMSEY_REFRESH_ITERS 256

/*
 * ramsey_refresh_ticks
 * --------------------
 * Calculate the number of ticks it takes for Ramsey to do multiple
 * DRAM refresh cycles. This result will then be used to determine the
 * input clock speed into Ramsey.
 */
static uint
ramsey_refresh_ticks(uint8_t control)
{
    uint16_t cia_ticks_start;
    uint16_t cia_ticks_end;
    uint8_t  ncontrol = control ^ RAMSEY_CONTROL_WRAP;
    uint     calltime;
    uint     count;

    cia_ticks_start = cia_ticks();  // force code into cache
    cia_ticks_start = cia_ticks();
    cia_ticks_end   = cia_ticks();
    calltime = cia_ticks_start - cia_ticks_end;

    /* Enable desired refresh rate (and synchronize with interval) */
    *ADDR8(RAMSEY_CONTROL) = control;
    while (*ADDR8(RAMSEY_CONTROL) != control)
        ;

    cia_ticks_start = cia_ticks();  // force code into cache
    cia_ticks_start = cia_ticks();

    /*
     * Enable and disable wrap multiple times.
     *
     * Note that this code is structured so that dcc will generate
     * the least number of instructions.
     */
    count = RAMSEY_REFRESH_ITERS;
    do {
        *ADDR8(RAMSEY_CONTROL) = ncontrol;
        while (*ADDR8(RAMSEY_CONTROL) != ncontrol)
            ;
        *ADDR8(RAMSEY_CONTROL) = control;
        while (*ADDR8(RAMSEY_CONTROL) != control)
            ;
        count -= 2;
    } while (count != 0);

    cia_ticks_end = cia_ticks();

    return (cia_ticks_start - cia_ticks_end - calltime);
}

static uint
cia_ticks(void)
{
    uint8_t hi1;
    uint8_t hi2;
    uint8_t lo;

    hi1 = *ADDR8(CIAA_TBHI);
    lo  = *ADDR8(CIAA_TBLO);
    hi2 = *ADDR8(CIAA_TBHI);

    /*
     * The below operation will provide the same effect as:
     *     if (hi2 != hi1)
     *         lo = 0xff;  // rollover occurred
     */
    lo |= (hi2 - hi1);  // rollover of hi forces lo to 0xff value

    return (lo | (hi2 << 8));
}

static uint
measure_ramsey_refreshes_per_ms(uint8_t control)
{
    uint8_t ocontrol = *ADDR8(RAMSEY_CONTROL);
    uint    count;
    uint    freq;
    uint    ticks;
    uint    refs;
    struct  EClockVal eclk;
    uint16_t (*ptr)(uint8_t control);
    uint    funclen = (uint) measure_ramsey_refreshes_per_ms -
                      (uint) ramsey_refresh_ticks + 16;

    ptr = AllocMem(funclen, MEMF_PUBLIC | MEMF_FAST);
    if (ptr != NULL) {
        CopyMem(ramsey_refresh_ticks, ptr, funclen);
        CacheClearE(ptr, funclen, CACRF_ClearD | CACRF_ClearI);
    }

    /*
     * Ensure that control has opposite wrap value of current state,
     * so a state change always occurs
     */
    control = (control & ~RAMSEY_CONTROL_WRAP) |
              ((ocontrol & RAMSEY_CONTROL_WRAP) ^ RAMSEY_CONTROL_WRAP);

    Disable();

    if (ptr != NULL) {
        ticks = ptr(control);
    } else {
#define AGNUS_DMA_DISABLE
#ifdef AGNUS_DMA_DISABLE
        /* Temporarily disable Agnus DMA */
        uint16_t dmacon = *ADDR16(AGNUS_DMACON_R);
        *ADDR16(AGNUS_DMACON_W) = 0x0100;  // Disable bit plane DMA
#endif
        ticks = ramsey_refresh_ticks(control);
#ifdef AGNUS_DMA_DISABLE
        *ADDR16(AGNUS_DMACON_W) = 0x8000 | dmacon;  // Re-enable bit plane DMA
#endif
    }

    /* Restore original refresh rate */
    *ADDR8(RAMSEY_CONTROL) = ocontrol;
    while (*ADDR8(RAMSEY_CONTROL) != ocontrol)
        ;

    Enable();

    freq = ReadEClock(&eclk);
    count = RAMSEY_REFRESH_ITERS;
    refs = freq * 1000 / ticks * count / 1000;
#if 0
    printf(" ticks=%u freq=%u count=%u refs=%u ", ticks, freq, count, refs);
    printf("ns=%u ", ticks * 100000 / count * 1000 / (freq / 10));
#endif
    /*
     * At 25 MHz and refresh mode 1 (240 clock cycles), a single refresh
     * will take 9.6 usecs. Ramsey must do a total of 512 refreshes to
     * hit all rows, which means all memory is getting refreshed over a
     * period of 4915.2 usecs. The 9.6 usec interval has been verified
     * with a logic analyzer.
     */
    if (ptr != NULL)
        FreeMem(ptr, funclen);
    return (refs);
}

/*
 * get_ramsey_clock
 * ----------------
 * Returns the calculated Ramsey clock speed in KHz
 */
const uint
get_ramsey_clock(void)
{
    uint8_t  ocontrol = get_ramsey_control();
    uint8_t  ncontrol;
    uint     cycles;
    uint     pass;
    uint     refs;
    uint     refs_max = 0;
    uint     index;

    index = ocontrol & (RAMSEY_CONTROL_REFRESH0 | RAMSEY_CONTROL_REFRESH1);
    ncontrol = ocontrol;

    switch (index) {
        case 0:
            cycles = 156;
            break;
        case RAMSEY_CONTROL_REFRESH0:
            cycles = 240;
            break;
        case RAMSEY_CONTROL_REFRESH1:
            cycles = 372;
            break;
        case RAMSEY_CONTROL_REFRESH0 | RAMSEY_CONTROL_REFRESH1:
            return (0);
    }

    Forbid();
    SUPERVISOR_STATE_ENTER();
    for (pass = 0; pass < 4; pass++) {
        refs = measure_ramsey_refreshes_per_ms(ncontrol);
        if (refs_max < refs)
            refs_max = refs;
    }
    SUPERVISOR_STATE_EXIT();
    Permit();

    return (cycles * refs_max / 1000);
}

/* ramsey_check() - verify the "expected" version of Ramsey is present */
static int
ramsey_check(void)
{
    ramsey_version = get_ramsey_version();

    switch (ramsey_version) {
        case 0x7f:
            ramsey_rev = 1;
            break;
        case 0x0d:
            ramsey_rev = 4;
            break;
        case 0x0f:
            ramsey_rev = 7;
            break;
        default:
            printf("Unrecognized Ramsey version $%x -- this program only works "
                   "on Amiga 3000\n", ramsey_version);
            return (1);
    }
    return (0);
}


static void
sc_memory_measure_refresh(void)
{
    uint8_t  ocontrol = get_ramsey_control();
    uint8_t  ncontrol;
    uint     refs0;
    uint     refs1;
    uint     refs2;

    ncontrol = ocontrol & ~(RAMSEY_CONTROL_REFRESH0 | RAMSEY_CONTROL_REFRESH1);

    Forbid();
    SUPERVISOR_STATE_ENTER();
    refs0 = measure_ramsey_refreshes_per_ms(ncontrol);
    refs1 = measure_ramsey_refreshes_per_ms(ncontrol | RAMSEY_CONTROL_REFRESH0);
    refs2 = measure_ramsey_refreshes_per_ms(ncontrol | RAMSEY_CONTROL_REFRESH1);
    SUPERVISOR_STATE_EXIT();
    Permit();

    /*
     * Ramsey Refresh cycles                  ----------measured------------
     * Index  Clocks    16 MHz      25 MHz    Clocks  16 MHz      25 MHz
     *   0    154       9.24 usec   6.16 usec 156     9.72 usec   6.240 usec
     *   1    238       14.28 usec  9.52 usec 240     15.00 usec  9.600 usec
     *   2    380       22.8 usec   15.2 usec 372     23.25 usec  14.88 usec
     *   3    infinite  -           -         -       -           -
     */
    printf("  (156)=%-7u (240)=%-7u (372)=%u\n", refs0, refs1, refs2);
}


/*
 * How to check for static column RAM (taken from Ramsey specification)
 * OLD VERSION:
 *
 * 1. Verify Ramsey version is not 0x7f (doesn't support SC RAM).
 * 2. Disable all interrupts and snapshot memory.
 * 3. Turn page mode on by setting the bit in the RAMSEY control register
 *    (read it back until the bit takes effect).
 * 4. Write $5AC35AC3, $AC35AC35, $C35AC35A, $35AC35AC to four consecutive
 *    longwords in the same page (to be in the same page, A11-A31 must be
 *    the same for all four longwords).
 * 5. Turn page mode off by resetting the bit in RAMSEY (wait for it to take
 *    effect).
 * 6. Compare the four longword values with what they were written with.
 *    If they are correct, then this bank of RAM has all static column DRAMs.
 * 7. Repeat steps 2 through 6 for each bank of Fast memory.
 * 8. Restore memory and re-enable interrupts.
 *
 * Since a refresh cycle will close the page, writes to the four longwords
 * of RAM must be less than 10 ?secs? apart.
 *
 * The above is out of data (no longer correct) since the A4000 came along.
 * On the A4000, if you do a write with burst mode enabled, and the memory
 * doesn't support burst mode, you can damage the hardware. Also, on the
 * 68040, page mode is required to be on.
 *
 * NEW VERSION:
 * 1. Verify Ramsey version is not 0x7f (doesn't support SC RAM).
 * 2. Disable all interrupts and snapshot memory.
 * 3. Disable cache, burst mode, and page mode.
 * 4. Write $5AC3A53C, $AC3A53C5, $C3A53C5A, $3A53C5AC to four consecutive
 *    longwords in the same page (to be in the same page, A11-A31 must be
 *    the same for all four longwords).
 * 5. Enable cache, burst mode, and page mode (68040).
 * 6. Read back the four consecutive longwords.
 * 7. Disable cache, burst mode, and page mode.
 * 6. Compare the four longword values with what they were written with.
 *    If they are correct, then this bank of RAM has all static column DRAMs.
 * 7. Repeat steps 2 through 6 for each bank of Fast memory.
 * 8. Restore memory and re-enable interrupts.
 */

static const uint32_t burst_magic[] = {
    0X5ac3a53c, 0Xac3a53c5, 0Xc3a53c5a, 0X3a53c5aC,
    0x11111111, 0x22222222, 0x44444444, 0x88888888,
    0xeeeeeeee, 0xdddddddd, 0xbbbbbbbb, 0x77777777,
    0x12345678, 0x23456789, 0x3456789a, 0x456789ab,
};

#define BURST_WORDS ARRAY_SIZE(burst_magic)

/*
 * sc_memory_probe_addr() probe all nibbles at the specified memory address
 *                        for Static Column support.
 */
static uint32_t
sc_memory_probe_addr(uint32_t addr, uint flags)
{
    uint8_t  ramsey_control_old;
    uint8_t  ramsey_control_burst;
    uint32_t save_data[BURST_WORDS];
    uint32_t got_data[BURST_WORDS];
    uint32_t word;
    uint32_t has_sc = BIT(8) - 1;  /* Assume all are Static Column */
    uint     nibble;
    uint     count;

    /* Disable Ramsey page mode */
    INTERRUPTS_DISABLE();
    ramsey_control_old = get_ramsey_control();
    ramsey_control_burst = ramsey_control_old | RAMSEY_CONTROL_BURST;
    if (cpu_type == 68040)
        ramsey_control_burst |= RAMSEY_CONTROL_PAGE;
    else
        ramsey_control_burst |= RAMSEY_CONTROL_WRAP;

    set_ramsey_control(ramsey_control_old &
                       ~(RAMSEY_CONTROL_BURST | RAMSEY_CONTROL_PAGE));

    /* Enable burst in CPU cache (no burst until Ramsey burst is enabled) */
    CACHE_ENABLE_DATA();
    CACHE_ENABLE_BURST();
    SUPERVISOR_STATE_ENTER();
    MMU_DISABLE();

    /* Save original data */
    memcpy(save_data, (void *) ADDR32(addr), sizeof (save_data));

    /* Fill with burst pattern */
    memcpy((void *) ADDR32(addr), burst_magic, sizeof (burst_magic));

    /* Ensure data lands in memory */
    cpu_dcache_flush();

    for (count = 0; count < ARRAY_SIZE(got_data) / 4; count++) {
        burst_test_read(&got_data[count * 4],
                        ADDR32(addr + count * 0x10), ramsey_control_burst);
    }

    /* Restore original data */
    memcpy((void *) ADDR32(addr), save_data, sizeof (save_data));

    MMU_RESTORE();
    cpu_dcache_flush();
    SUPERVISOR_STATE_EXIT();
    CACHE_RESTORE_STATE();
    INTERRUPTS_ENABLE();

    /* Check for match of expected values */
    for (word = 0; word < BURST_WORDS; word++) {
        for (nibble = 0; nibble < 8; nibble++) {
            uint nibble_value = (got_data[word] >> (nibble * 4)) & 0xf;
            uint sc_expected = (burst_magic[word] >> (nibble * 4)) & 0xf;
            if (nibble_value != sc_expected) {
                /* Nibble did not match -- not Static Column */
                has_sc &= ~BIT(nibble);
            }
        }
    }
    if (flags & FLAG_DEBUG) {
        printf("%08x %08x %08x %08x\n",
               got_data[0], got_data[1], got_data[2], got_data[3]);
    }

    return (has_sc);
}

static int
get_mem_types(uint addrbits, uint32_t *bank_results, uint flags)
{
    uint bank;

    if (!cpu_can_do_burst)
        return (1);

    memset(bank_results, 0, sizeof (*bank_results) * ZIP_BANKS);

    /* Generate addresses */
    for (bank = 0; bank < ZIP_BANKS; bank++) {
        /* Probe all nibbles in bank at the same time */
        uint32_t addr = amask_to_address(bank, 0, addrbits) & ~0xff;
        bank_results[bank] = sc_memory_probe_addr(addr, flags);
    }
    return (0);
}

/*
 * sc_memory_probe() - probe all memory for Static Column support
 *
 * Static column memory supports burst read/write operations from the CPU.
 * Unfortunately, this burst mode doesn't seem to be supported by most
 * accelerators (A3640 for example), so the test will only give reliable
 * results with the onboard 68030.
 *
 * The test works by capturing memory contents, patterning those contents
 * with a burst write, reading those pattern contents back, restoring the
 * original contents, and then comparing the pattern contents with what
 * was written.
 */
static void
sc_memory_probe(uint addrbits, uint flags)
{
    uint     pos;
    uint     probed_banks = 0;
    uint32_t bank_results[ZIP_BANKS];
    struct   EClockVal eclk;
    uint     freq;
    static const char socket_l1[] = "Socket   ADDR    Type";
    static const char socket_l2[] = "-------- ------- ----";
    uint8_t ramsey_version = get_ramsey_version();

    if (ramsey_version == 0x7f) {
        printf("Ramsey-01 does not support SC RAM\n");
        return;
    }

    freq = ReadEClock(&eclk);
    printf("Ramsey refreshes / second measured using EClock=%u.%02u KHz\n",
           freq / 1000, freq % 1000);
    for (pos = 0; pos < 8; pos++)
        sc_memory_measure_refresh();
    printf("\n");

    sc_memory_speed();
    if (get_mem_types(addrbits, bank_results, flags)) {
        printf("The installed CPU does not support burst and so it's not\n"
               "possible to correctly detect installed ZIP memory type.\n");
        return;
    }

    printf("Static Column Test\n");
    printf("  %s  %s\n"
           "  %s  %s\n", socket_l1, socket_l1, socket_l2, socket_l2);

    /* Generate addresses */
    for (pos = 0; pos < ARRAY_SIZE(zip_u_data); pos++) {
        uint     bank   = zip_u_data[pos].bank;
        uint     nibble = zip_u_data[pos].nibble;
        uint32_t addr   = amask_to_address(bank, 0, addrbits) & ~0xff;
        char    *dram_type;

        printf("  %s %u.%u %07x ", zip_u_data[pos].skt, zip_u_data[pos].bank,
               nibble, addr + nibble / 2);

        dram_type = (bank_results[bank] & BIT(nibble)) ? "SC" : "FPM";
        if (zip_u_data[pos].position == POS_RIGHT)
            printf("%s\n", dram_type);
        else
            printf("%-4s", dram_type);
    }
}

/*
 * gen_address_strobes() - generate address strobes on the ZIP memory bus
 *
 * This function is really only useful for scope/analyzer probe purposes.
 *
 * It walks all ZIP memory banks, causing reads at addresses which correspond
 * to power-of-two RAS and CAS addresses.  You will see two back-to-back reads.
 * One is the power-of-two RAS and CAS address, and the following one is the
 * inverted address.  Before starting accesses, the function will also drive
 * high all parallel port data lines.  This is useful as a start trigger for
 * the analyzer.
 */
static void
gen_address_strobes(uint addrbits, uint flags)
{
    uint     inner;
    uint     iter;
    uint     addrbit;
    uint     bank;
    uint32_t amask0;
    uint32_t amask1;
    uint32_t all_bits = BIT(addrbits) - 1;
    uint32_t addr0[ZIP_BANKS][21];
    uint32_t addr1[ZIP_BANKS][21];

    CACHE_DISABLE_DATA();
    CACHE_RESTORE_STATE();

    /* Generate addresses */
    for (bank = 0; bank < ZIP_BANKS; bank++) {
        for (addrbit = 0; addrbit <= addrbits; addrbit++) {
            if (addrbit == 0) {
                amask0  = 0;
                amask1 = all_bits;
            } else {
                amask0 = BIT(addrbit - 1);
                amask1 = all_bits ^ amask0;
            }
            addr0[bank][addrbit] = amask_to_address(bank, amask0, addrbits);
            addr1[bank][addrbit] = amask_to_address(bank, amask1, addrbits);

            if (flags & FLAG_DEBUG)
                printf("%06x=%x %06x=%x\n", amask0, addr0[bank][addrbit],
                       amask1, addr1[bank][addrbit]);
        }
    }

    if (flags & FLAG_DEBUG)
        printf("(parport pins high during strobes)\n");

    *ADDR8(AMIGA_PPORT_DIR)  = 0xff;

    for (iter = 0; iter <= 20; iter++) {
        CACHE_DISABLE_DATA();
        SUPERVISOR_STATE_ENTER();
        INTERRUPTS_DISABLE();
        MMU_DISABLE();
        *ADDR8(AMIGA_PPORT_DATA) = 0xff;
        for (bank = 0; bank < ZIP_BANKS; bank++) {
            uint32_t *ptr0 = &addr0[bank][0];
            uint32_t *ptr1 = &addr1[bank][0];
            for (addrbit = 0; addrbit <= addrbits; addrbit++) {
                for (inner = 0; inner < 2; inner++) {
                    uint32_t dummy;
                    dummy = *ADDR32(*ptr0);
                    dummy = *ADDR32(*ptr1);
                }
                ptr0++;
                ptr1++;
            }
        }
        *ADDR8(AMIGA_PPORT_DATA) = 0x00;
        MMU_RESTORE();
        INTERRUPTS_ENABLE();
        SUPERVISOR_STATE_EXIT();
        CACHE_RESTORE_STATE();
    }
}

/* Test patterns (must be a prime number of patterns) */
static const uint32_t cell_patterns[] = {
    0xaaaaaaaa, 0x55555555, 0xcccccccc, 0x33333333,
    0x11111111, 0x22222222, 0x44444444, 0x88888888,
    0x77777777, 0xeeeeeeee, 0xdddddddd, 0xbbbbbbbb,
    0x00000000
};

/*
 * pattern_check_mem() - run a pattern test on the specified memory range
 */
static uint32_t
pattern_check_mem(volatile uint32_t *addr, size_t size, uint flags)
{
    volatile uint32_t *taddr;
    uint32_t           biterr = 0;
    uint               pat;
    size_t             count;
    uint               iters = ARRAY_SIZE(cell_patterns);
    uint               iter;

    if (!(flags & FLAG_LONG_TEST))
        iters = 2;
    for (iter = 0; iter < iters; iter++) {
        /* Write pattern set */
        pat   = iter;
        taddr = addr;
        count = size / 4;
        while (count-- > 0) {
            *taddr = cell_patterns[pat];
            if (++pat == iters)
                pat = 0;
            taddr++;
        }

        cpu_dcache_flush();

        /* Verify pattern set */
        pat   = iter;
        taddr = addr;
        count = size / 4;
        while (count-- > 0) {
            biterr |= (*taddr ^ cell_patterns[pat]);
            if (++pat == iters)
                pat = 0;
            taddr++;
        }
    }
    return (biterr);
}

/*
 * cell_data_test() - test all ZIP package memory cells
 *
 * Algoritm:
 * 1) Allocate 4k buffer in chip memory
 * 2) Walk all banks:
 * 3) Disable interrupts
 * 4) Walk all memory blocks..
 * 5) Walk list of memory cell test patterns, writing different pattern to
 *    each consecutive memory location.
 * 6) Flush cache
 * 7) Verify list of patterns against memory locations
 * 8) Execute above repeatedly, using each pattern as a new starting point
 * 9) Enable interrupts
 */
static int
cell_data_test(uint32_t bank_size, uint flags)
{
    int       errs = 0;
    uint      pos;
    uint      bank;
    uint32_t *save_data = AllocMem(TESTBLOCK_SIZE, MEMF_PUBLIC | MEMF_CHIP);
    uint32_t *diffs     = AllocMem(TESTBLOCK_SIZE, MEMF_PUBLIC | MEMF_CHIP);
    uint8_t   bad_chips[ZIP_BANKS][8];  /* [banks][nibbles] */

    printf("Memory cell test\n");
    memset(bad_chips, 0, sizeof (bad_chips));

    if ((save_data == NULL) || (diffs == NULL)) {
        printf("Cannot allocate chip memory for test buffer\n");
        goto cleanup;
    }

    memset(diffs, 0, TESTBLOCK_SIZE);

    /* Perform test */
    for (bank = 0; bank < ZIP_BANKS; bank++) {
        uint32_t start  = FASTMEM_TOP - bank_size * (bank + 1);
        uint32_t end    = FASTMEM_TOP - bank_size * bank;
        uint32_t addr   = start;
        uint     goterr = 0;

        if (flags & FLAG_DEBUG)
            printf("\nstart=%x end=%x\n", start, end);
        printf("  Bank %u [%*s]\r  Bank %u [",
               bank, bank_size / 0x20000, "", bank);

        CACHE_DISABLE_DATA();
        for (addr = start; addr < end; addr += TESTBLOCK_SIZE) {
            uint32_t biterr;

            /* Cache and interrupts are disabled in this block */
            SUPERVISOR_STATE_ENTER();
//          INTERRUPTS_DISABLE();
            irq_disable();
            MMU_DISABLE();
            burst_copy(save_data, (void *) ADDR32(addr), TESTBLOCK_SIZE);
            biterr = pattern_check_mem(ADDR32(addr), 2048, flags);
            burst_copy((void *) ADDR32(addr), save_data, TESTBLOCK_SIZE);
            cpu_dcache_flush();
            MMU_RESTORE();
            cpu_dcache_flush();
            irq_enable();
//          INTERRUPTS_ENABLE();
            SUPERVISOR_STATE_EXIT();

            if (biterr != 0) {
                uint nibble;
                if ((errs++ < 10) && (flags & FLAG_DEBUG))
                    printf("err=%08x at %06x\n", biterr, addr);
                for (nibble = 0; nibble < 8; nibble++) {
                    if (biterr & 0xf)
                        bad_chips[bank][nibble] = 1;
                    biterr >>= 4;
                }
                goterr++;
            }
            if ((addr & 0x1ffff) == 0) {
                uint nibble;
//              printf("[%04x %04x %04x]", tval1, tval2, tval3);
                printf("%c", goterr ? 'X' : '.');
                fflush(stdout);

                /* Quit early if all nibbles in this bank are bad */
                if (goterr) {
                    for (nibble = 0; nibble < 8; nibble++)
                        if (bad_chips[bank][nibble] == 0)
                            break;
                    if (nibble == 8)
                        break;
                }
                goterr = 0;
            }
        }
        CACHE_RESTORE_STATE();
        if (addr >= end)
            printf("]");
        printf("\n");
    }
    printf("\n");

    /* Display results */
    printf("  Socket   Result   Socket   Result\n"
           "  -------- ------   -------- ------\n");
    for (pos = 0; pos < ARRAY_SIZE(zip_u_data); pos++) {
        uint nibble = zip_u_data[pos].nibble;
        bank = zip_u_data[pos].bank;
        printf("  %s %u.%u %-4s", zip_u_data[pos].skt, zip_u_data[pos].bank,
               nibble, bad_chips[bank][nibble] ? "!" : "Good");
        if (zip_u_data[pos].position == POS_RIGHT)
            printf("\n");
        else
            printf("   ");
    }

    if (flags & FLAG_SHOW_DIP) {
        int nibble;
        bank = dip_u_data[0].bank;
        show_dip_header();
        printf("     ");
        for (nibble = 7; nibble >= 0; nibble--)
            printf(" %-5s", bad_chips[bank][nibble] ? "!" : "Good");
        printf("\n");
    }

cleanup:
    if (diffs != NULL)
        FreeMem(diffs, TESTBLOCK_SIZE);
    if (save_data != NULL)
        FreeMem(save_data, TESTBLOCK_SIZE);
    return (errs);
}

/*
 * section_verify() - report if specified address is not in chip memory
 */
static int
section_verify(const char *str, const void *ptr)
{
    if ((uint32_t) ptr >= 0x200000) {
        /* Outside chip memory range */
        printf("ERROR: %s=0x%07x is not in CHIP memory\n", str, (uint32_t) ptr);
        return (1);
    }
    return (0);
}

static void
mmu_open(void)
{
    uint32_t tc;

    if (cpu_type == 68030) {
        SUPERVISOR_STATE_ENTER();
        tc = mmu_get_tc_030();              // pmove.l tc,(sp)
        SUPERVISOR_STATE_EXIT();
        mmu_is_active = !!(tc & BIT(31));
    }
    if ((cpu_type == 68040) || (cpu_type == 68060)) {
        SUPERVISOR_STATE_ENTER();
        tc = mmu_get_tc_040();              // movec.l tc,d0
        SUPERVISOR_STATE_EXIT();
        mmu_is_active = !!(tc & BIT(15));
    }
}

/*
 * c_main() - parse and execute arguments
 */
int
c_main(int argc, char **argv)
{
    uint32_t bank_size;           /* Memory bank size (1MB or 4MB) */
    uint8_t  mem_control;         /* Memory control register value (Ramsey) */
    uint8_t  mem_refresh;         /* Memory refresh rate code (0, 1, 2, 3) */
    uint8_t  mem_width;           /* Bits per ZIP IC: 1 or 4 (4 is expected) */
    uint     mem_addrbits;        /* ZIP memory bits (20 or 18) */
    uint     ramsey_khz;
    int      arg;
    int      rc2;
    int      rc             = 0;
    int      comma          = 0;
    uint     skip_mode      = 0;
    uint     flags          = 0;
    int      flag_addr_test = 0;  /* Address line test */
    int      flag_cell_test = 0;  /* Memory cell test */
    int      flag_data_test = 0;  /* Data line test */
    int      flag_info      = 0;  /* Only show system info */
    int      flag_force     = 0;  /* Ignore the fact that enforcer is present */
    int      flag_quiet     = 0;  /* Don't display banner */
    int      flag_strobe    = 0;  /* Generate address strobes for logic probe */
    int      flag_sprobe    = 0;  /* Probe for static column memory */

    for (arg = 1; arg < argc; arg++) {
        if (stricmp(argv[arg], "ADDR") == 0) {
            flag_addr_test = 1;
        } else if (stricmp(argv[arg], "ASCII") == 0) {
            show_ascii_art();
            return (0);
        } else if (stricmp(argv[arg], "CELL") == 0) {
            flag_cell_test = 1;
        } else if (stricmp(argv[arg], "DATA") == 0) {
            flag_data_test = 1;
        } else if (stricmp(argv[arg], "DEBUG") == 0) {
            if (flags & FLAG_DEBUG)
                flags |= FLAG_MORE_DEBUG;
            else
                flags |= FLAG_DEBUG;
        } else if (stricmp(argv[arg], "DIP") == 0) {
            flags |= FLAG_SHOW_DIP;
        } else if (stricmp(argv[arg], "FORCE") == 0) {
            flag_force = 1;
        } else if (stricmp(argv[arg], "INFO") == 0) {
            flag_info = 1;
        } else if (stricmp(argv[arg], "LONG") == 0) {
            flags |= FLAG_LONG_TEST;
        } else if (stricmp(argv[arg], "MAP") == 0) {
            flags |= FLAG_SHOW_MAP;
        } else if (stricmp(argv[arg], "QUIET") == 0) {
            flag_quiet = 1;
        } else if (stricmp(argv[arg], "SPROBE") == 0) {
            flag_sprobe = 1;
        } else if (stricmp(argv[arg], "STROBE") == 0) {
            flag_strobe = 1;
        } else {
            usage();
            return (1);
        }
    }
    if (!flag_quiet)
        printf("%s\n", version + 7);

    cpu_type = get_cpu();
    mmu_open();
    if (!flag_quiet) {
        cpu_can_do_burst = cpu_can_burst();
        printf("CPU: %u %s Burst%s\n", cpu_type,
               cpu_can_do_burst ? "with" : "without",
               mmu_is_active ? ", MMU Active" : "");
    }

    if (!flag_force && enforcer_check())
        return (1);
    if (ramsey_check() && !flag_force)
        return (1);

    /* Use bitwise OR here so that all sections are checked and reported */
    if (section_verify("sp", &rc) |
        section_verify("pc", (void *) c_main) |
        section_verify("rodata", dip_u_data)) {
        return (1);
    }

    ramsey_version = get_ramsey_version();
    mem_control    = get_ramsey_control();
    mem_refresh    = (mem_control >> 5) & 3;  // RAMSEY_CONTROL_REFRESH0
    mem_addrbits   = (mem_control & RAMSEY_CONTROL_RAMSIZE) ? 20 : 18;
    if (ramsey_version == 0x0d) {
        /* Ramsey-04 */
        mem_width = (mem_control & RAMSEY_CONTROL_RAMWIDTH) ? 4 : 1;
        skip_mode = 0;
    } else {
        /* Ramsey-07 */
        skip_mode = !!(mem_control & RAMSEY_CONTROL_SKIP);
        mem_width = 4;  // x1 RAM support removed, cycle skip mode added
    }
    bank_size  = BIT(mem_addrbits) * mem_width;

    if (flag_info || !flag_quiet) {
        ramsey_khz = get_ramsey_clock() + 5;  // round up
        printf("Memory controller: Ramsey-0%d $%x $%02x (%u.%02u MHz)\n",
                ramsey_rev, ramsey_version, get_ramsey_control(),
                ramsey_khz / 1000, (ramsey_khz % 1000) / 10);
        printf("Memory config: %sx%u (%u%cB per bank)",
               (mem_addrbits == 20) ? "1M" : "256", mem_width,
               (bank_size >> 20) ? (bank_size >> 20) : (bank_size >> 10),
               (bank_size >> 20) ? 'M' : 'K');

        if (mem_control & RAMSEY_CONTROL_PAGE) {
            printf(" Page");
            comma = 1;
        }
        if (mem_control & RAMSEY_CONTROL_BURST) {
            if (comma++)
                printf(",");
            printf(" Burst");
        }
        if (mem_control & (RAMSEY_CONTROL_PAGE | RAMSEY_CONTROL_BURST)) {
            printf(" (SCRAM required)");
        }
        if (mem_control & RAMSEY_CONTROL_WRAP) {
            if (comma++)
                printf(",");
            printf(" Wrap");
        }
        if (skip_mode) {
            /* Ramsey-07 offers Skip mode with 60ns RAM */
            if (comma)
                printf(",");
            printf(" Skip");
        }
        printf("\nMemory refresh: %s clocks (%s)\n",
               ramsey_refresh_timing[mem_refresh].clocks,
               (ramsey_khz < 20000) ?
               ramsey_refresh_timing[mem_refresh].interval_16m :
               ramsey_refresh_timing[mem_refresh].interval_25m);
    }

    if (!flag_addr_test && !flag_data_test && !flag_cell_test &&
        !flag_strobe && !flag_sprobe) {
        flag_addr_test = 1;
        flag_data_test = 1;
        flag_cell_test = 1;
    }
    if (flag_info) {
        return (0);
    }

#ifdef TEST_BANK_AMASK_TO_ADDRESS
    selftest_bank_amask_to_address();
    return (0);
#endif
    if (flag_strobe) {
        gen_address_strobes(mem_addrbits, flags);
        return (0);
    }

    if (flags & FLAG_SHOW_MAP) {
        (void) data_line_test(mem_addrbits, flags);
        address_line_map(mem_addrbits);
        return (0);
    }

    if (flag_data_test) {
        printf("\n");
        rc = data_line_test(mem_addrbits, flags);
    }

    if (flag_addr_test) {
        printf("\n");
        rc2 = address_line_test(mem_addrbits, flags);
        if (rc == 0)
            rc = rc2;
    }

    if (flag_sprobe) {
        printf("\n");
        sc_memory_probe(mem_addrbits, flags);
    }

    if (flag_cell_test) {
        printf("\n");
        rc2 = cell_data_test(bank_size, flags);
        if (rc == 0)
            rc = rc2;
    }
    return (rc);
}
