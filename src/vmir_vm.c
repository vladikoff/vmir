/*
 * Copyright (c) 2016 Lonelycoder AB
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//#define VM_TRACE

#ifndef VM_TRACE
#define VM_USE_COMPUTED_GOTO
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifdef VM_TRACE
#define vm_printf(fmt...) printf(fmt)
#else
#define vm_printf(fmt...)
#endif

#define IR_MEM_ADDR(iec, addr) ((iec)->memory + addr)


static void __attribute__((noinline)) __attribute__((noreturn))
vm_stop(ir_unit_t *iu, int reason, int code)
{
  iu->iu_exit_code = code;
  longjmp(iu->iu_err_jmpbuf, reason);
}


static uint32_t
vm_arg32(const void **rfp)
{
  const void *rf = *rfp = *rfp - 4;
  return *(uint32_t *)rf;
}

static uint64_t
vm_arg64(const void **rfp)
{
  const void *rf = *rfp = *rfp - 8;
  return *(uint64_t *)rf;
}

static double
vm_arg_dbl(const void **rfp)
{
  const void *rf = *rfp = *rfp - 8;
  return *(double *)rf;
}

static float __attribute__((unused))
vm_arg_flt(const void **rfp)
{
  const void *rf = *rfp = *rfp - 4;
  return *(float *)rf;
}

static void *
vm_ptr(const void **rfp, ir_unit_t *iu)
{
  return vm_arg32(rfp) + iu->iu_mem;
}

static void
vm_retptr(void *ret, void *p, const ir_unit_t *iu)
{
  *(uint32_t *)ret = p ? p - iu->iu_mem : 0;
}

static void
vm_retNULL(void *ret)
{
  *(uint32_t *)ret = 0;
}

static void
vm_ret32(void *ret, uint32_t v)
{
  *(uint32_t *)ret = v;
}


static void
vm_exit(void *ret, const void *rf, ir_unit_t *iu)
{
  uint32_t exit_code = vm_arg32(&rf);
  vm_stop(iu, VM_STOP_EXIT, exit_code);
}

static void
vm_abort(void *ret, const void *rf, ir_unit_t *iu)
{
  vm_stop(iu, VM_STOP_ABORT, 0);
}


static uint32_t __attribute__((noinline))
vm_strchr(uint32_t a, int b, void *mem)
{
  void *s = mem + a;
  void *r = strchr(s, b);
  int ret = r ? r - mem : 0;
  vm_printf("strchr(%s (@ 0x%x), %c) = 0x%x\n", (char *)s, a, b, ret);
  return ret;
}

static uint32_t __attribute__((noinline))
vm_strrchr(uint32_t a, int b, void *mem)
{
  void *s = mem + a;
  void *r = strrchr(s, b);
  int ret = r ? r - mem : 0;
  vm_printf("strrchr(%s (@ 0x%x), %c) = 0x%x\n", (char *)s, a, b, ret);
  return ret;
}


static uint32_t __attribute__((noinline))
vm_vaarg32(void *rf, void **ptr)
{
  void *p = *ptr;
  p -= sizeof(uint32_t);
  uint32_t r = *(uint32_t *)p;
  *ptr = p;
  return r;
}


static uint64_t __attribute__((noinline))
vm_vaarg64(void *rf, void **ptr)
{
  void *p = *ptr;
  p -= sizeof(uint64_t);
  uint64_t r = *(uint64_t *)p;
  *ptr = p;
  return r;
}

#ifdef VM_TRACE
static void __attribute__((noinline))
vm_wr_u8(void *rf, int16_t reg, uint8_t data, int line)
{
  vm_printf("Reg 0x%x (u8) = 0x%x by %d\n", reg, data, line);
  *(uint8_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_s8(void *rf, int16_t reg, int8_t data, int line)
{
  vm_printf("Reg 0x%x (s8) = 0x%x by %d\n", reg, data, line);
  *(int8_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_u16(void *rf, int16_t reg, uint16_t data, int line)
{
  vm_printf("Reg 0x%x (u16) = 0x%x by %d\n", reg, data, line);
  *(uint16_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_s16(void *rf, int16_t reg, int16_t data, int line)
{
  vm_printf("Reg 0x%x (s16) = 0x%x by %d\n", reg, data, line);
  *(int16_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_u32(void *rf, int16_t reg, uint32_t data, int line)
{
  vm_printf("Reg 0x%x (u32) = 0x%x by %d\n", reg, data, line);
  *(uint32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_s32(void *rf, int16_t reg, int32_t data, int line)
{
  vm_printf("Reg 0x%x (s32) = 0x%x by %d\n", reg, data, line);
  *(int32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_u64(void *rf, int16_t reg, uint64_t data, int line)
{
  vm_printf("Reg 0x%x (u64) = 0x%"PRIx64" by %d\n", reg, data, line);
  *(uint64_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_s64(void *rf, int16_t reg, int64_t data, int line)
{
  vm_printf("Reg 0x%x (s8) = 0x%"PRIx64" by %d\n", reg, data, line);
  *(int64_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_flt(void *rf, int16_t reg, float data, int line)
{
  vm_printf("Reg 0x%x (flt) = %f by %d\n", reg, data, line);
  *(float *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_wr_dbl(void *rf, int16_t reg, double data, int line)
{
  vm_printf("Reg 0x%x (dbl) = %f by %d\n", reg, data, line);
  *(double *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_8(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint8_t data = *(uint8_t *)(mem + ea);
  vm_printf("Reg 0x%x (u8) = Loaded 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint8_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_8_zext_32(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint8_t data = *(uint8_t *)(mem + ea);
  vm_printf("Reg 0x%x (u32) = Loaded.u8 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_8_sext_32(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  int8_t data = *(int8_t *)(mem + ea);
  vm_printf("Reg 0x%x (u32) = Loaded.s8 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(int32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_16(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint16_t data = *(uint16_t *)(mem + ea);
  vm_printf("Reg 0x%x (u16) = Loaded 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint16_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_16_zext_32(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint16_t data = *(uint16_t *)(mem + ea);
  vm_printf("Reg 0x%x (u32) = Loaded.u16 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_16_sext_32(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  int16_t data = *(int16_t *)(mem + ea);
  vm_printf("Reg 0x%x (u32) = Loaded.s16 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(int32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_32(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint32_t data = *(uint32_t *)(mem + ea);
  vm_printf("Reg 0x%x (u32) = Loaded 0x%x from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint32_t *)(rf + reg) = data;
}

static void __attribute__((noinline))
vm_load_64(void *rf, int16_t reg, void *mem, uint32_t ea, int line)
{
  uint64_t data = *(uint64_t *)(mem + ea);
  vm_printf("Reg 0x%x (u64) = Loaded 0x%"PRIx64" from 0x%08x by %d\n",
         reg, data, ea, line);
  *(uint64_t *)(rf + reg) = data;
}


static void __attribute__((noinline))
vm_store_8(void *mem, uint32_t ea, uint8_t v, int line)
{
  vm_printf("Store (u8) 0x%x to 0x%08x by %d\n", v, ea, line);
  *(uint8_t *)(mem + ea) = v;
}

static void __attribute__((noinline))
vm_store_16(void *mem, uint32_t ea, uint16_t v, int line)
{
  vm_printf("Store (u16) 0x%x to 0x%08x by %d\n", v, ea, line);
  *(uint16_t *)(mem + ea) = v;
}

static void __attribute__((noinline))
vm_store_32(void *mem, uint32_t ea, uint32_t v, int line)
{
  vm_printf("Store (u32) 0x%x to 0x%08x by %d\n", v, ea, line);
  *(uint32_t *)(mem + ea) = v;
}

static void __attribute__((noinline))
vm_store_64(void *mem, uint32_t ea, uint64_t v, int line)
{
  vm_printf("Store (u64) 0x%"PRIx64" to 0x%08x by %d\n", v, ea, line);
  *(uint64_t *)(mem + ea) = v;
}


static const char *
vm_funcname(int callee, ir_unit_t *iu)
{
  if(callee >= VECTOR_LEN(&iu->iu_functions))
    return "Bad-function-global-id";
  ir_function_t *f = VECTOR_ITEM(&iu->iu_functions, callee);
  return f->if_name;
}

#endif



#define ACCPOS 8
#define R32_ACC *(uint32_t *)(rf + ACCPOS)
#define S32_ACC *(int32_t  *)(rf + ACCPOS)

#define R8(r)  *(uint8_t  *)(rf + (int16_t)I[r])
#define S8(r)  *(int8_t   *)(rf + (int16_t)I[r])
#define R16(r) *(uint16_t *)(rf + (int16_t)I[r])
#define S16(r) *(int16_t  *)(rf + (int16_t)I[r])
#define R32(r) *(uint32_t *)(rf + (int16_t)I[r])
#define S32(r) *(int32_t  *)(rf + (int16_t)I[r])
#define R64(r) *(uint64_t *)(rf + (int16_t)I[r])
#define S64(r) *(int64_t  *)(rf + (int16_t)I[r])
#define RFLT(r)  *(float  *)(rf + (int16_t)I[r])
#define RDBL(r)  *(double *)(rf + (int16_t)I[r])


#ifdef VM_TRACE

#define AR8(reg, src)  vm_wr_u8(rf,  I[reg], src, __LINE__)
#define AS8(reg, src)  vm_wr_s8(rf,  I[reg], src, __LINE__)
#define AR16(reg, src) vm_wr_u16(rf, I[reg], src, __LINE__)
#define AS16(reg, src) vm_wr_s16(rf, I[reg], src, __LINE__)
#define AR32(reg, src) vm_wr_u32(rf, I[reg], src, __LINE__)
#define AS32(reg, src) vm_wr_s32(rf, I[reg], src, __LINE__)
#define AR64(reg, src) vm_wr_u64(rf, I[reg], src, __LINE__)
#define AS64(reg, src) vm_wr_s64(rf, I[reg], src, __LINE__)
#define AFLT(reg, src) vm_wr_flt(rf, I[reg], src, __LINE__)
#define ADBL(reg, src) vm_wr_dbl(rf, I[reg], src, __LINE__)

#define AR32_ACC(src) vm_wr_u32(rf, ACCPOS, src, __LINE__)
#define AS32_ACC(src) vm_wr_s32(rf, ACCPOS, src, __LINE__)


#define LOAD8(reg, ea)       vm_load_8(rf,  I[reg], mem, ea, __LINE__)
#define LOAD8_ZEXT_32(r, ea) vm_load_8_zext_32(rf,  I[r], mem, ea, __LINE__)
#define LOAD8_SEXT_32(r, ea) vm_load_8_sext_32(rf,  I[r], mem, ea, __LINE__)

#define LOAD16(reg, ea)       vm_load_16(rf, I[reg], mem, ea, __LINE__)
#define LOAD16_ZEXT_32(r, ea) vm_load_16_zext_32(rf,  I[r], mem, ea, __LINE__)
#define LOAD16_SEXT_32(r, ea) vm_load_16_sext_32(rf,  I[r], mem, ea, __LINE__)


#define LOAD32(reg, ea)  vm_load_32(rf, I[reg], mem, ea, __LINE__)
#define LOAD64(reg, ea)  vm_load_64(rf, I[reg], mem, ea, __LINE__)

#define STORE8(ea, v)    vm_store_8(mem, ea, v, __LINE__)
#define STORE16(ea, v)   vm_store_16(mem, ea, v, __LINE__)
#define STORE32(ea, v)   vm_store_32(mem, ea, v, __LINE__)
#define STORE64(ea, v)   vm_store_64(mem, ea, v, __LINE__)


#else

#define LOAD8(r, ea)   R8(r)  = *(uint8_t *)MEM(ea)
#define LOAD8_ZEXT_32(r, ea)   R32(r)  = *(uint8_t *)MEM(ea)
#define LOAD8_SEXT_32(r, ea)   S32(r)  = *(int8_t *)MEM(ea)
#define LOAD16(r, ea)  R16(r) = *(uint16_t *)MEM(ea)
#define LOAD16_ZEXT_32(r, ea)   R32(r)  = *(uint16_t *)MEM(ea)
#define LOAD16_SEXT_32(r, ea)   S32(r)  = *(int16_t *)MEM(ea)
#define LOAD32(r, ea)  R32(r) = *(uint32_t *)MEM(ea)
#define LOAD64(r, ea)  R64(r) = *(uint64_t *)MEM(ea)
#define STORE8(ea, v)   *(uint8_t *)MEM(ea) = v
#define STORE16(ea, v)  *(uint16_t *)MEM(ea) = v
#define STORE32(ea, v)  *(uint32_t *)MEM(ea) = v
#define STORE64(ea, v)  *(uint64_t *)MEM(ea) = v

#define AR8(r, src)  R8(r) = src
#define AS8(r, src)  S8(r) = src
#define AR16(r, src) R16(r) = src
#define AS16(r, src) S16(r) = src
#define AR32(r, src) R32(r) = src
#define AS32(r, src) S32(r) = src
#define AR32_ACC(src) R32_ACC = src
#define AS32_ACC(src) S32_ACC = src
#define AR64(r, src) R64(r) = src
#define AS64(r, src) S64(r) = src
#define AFLT(r, src) RFLT(r) = src
#define ADBL(r, src) RDBL(r) = src

#endif




#define UIMM8(r) *(uint8_t *)(I + r)
#define SIMM8(r) *(int8_t *)(I + r)
#define UIMM16(r) *(uint16_t *)(I + r)
#define SIMM16(r) *(int16_t *)(I + r)
#define UIMM32(r) *(uint32_t *)(I + r)
#define SIMM32(r) *(int32_t *)(I + r)
#define UIMM64(r) *(uint64_t *)(I + r)
#define SIMM64(r) *(int64_t *)(I + r)

#define IMMFLT(r) *(float *)(I + r)
#define IMMDBL(r) *(double *)(I + r)

#define MEM(x) ((mem) + (x))


static void * __attribute__((noinline))
do_jit_call(void *rf, void *mem, void *(*code)(void *, void *))
{
#if 0
  printf("%p: Pre jit call 8=%x c=%x 10=%x\n", code,
         *(uint32_t *)(rf + 8),
         *(uint32_t *)(rf + 12),
         *(uint32_t *)(rf + 16));
#endif
  void *r = code(rf, mem);
#if 0
  printf("%p: Post jit call 8=%x c=%x 10=%x\n", code,
         *(uint32_t *)(rf + 8),
         *(uint32_t *)(rf + 12),
         *(uint32_t *)(rf + 16));
#endif
  return r;
}

static int __attribute__((noinline))
vm_exec(const uint16_t *I, void *rf, ir_unit_t *iu, void *ret,
        uint32_t allocaptr, vm_op_t op)
{
  int16_t opc;

#ifdef VM_USE_COMPUTED_GOTO
  if((int)op != -1)
    goto resolve;
  void *mem = iu->iu_mem;

#define NEXT(skip) I+=skip; opc = *I++; goto *(&&opz + opc)
#define VMOP(x) x:

  NEXT(0);

  while(1) {

  opz:
    vm_stop(iu, VM_STOP_BAD_INSTRUCTION, 0);
#else

  if((int)op != -1)
    return op; // Resolve to itself when we use switch() { case ... }
  void *mem = iu->iu_mem;

#define NEXT(skip) I+=skip; opc = *I++; goto reswitch

#ifdef VM_TRACE
#define VMOP(x) case VM_ ## x : printf("%s %04x %04x %04x %04x %04x %04x %04x %04x\n", #x, \
  I[0], I[1], I[2], I[3], I[4], I[5], I[6], I[7]);
#else
#define VMOP(x) case VM_ ## x :
#endif

  opc = *I++;
 reswitch:
  switch(opc) {
  default:
    vm_stop(iu, VM_STOP_BAD_INSTRUCTION, 0);
#endif

  VMOP(NOP)
    NEXT(0);

  VMOP(RET_VOID)
    return 0;

  VMOP(JIT_CALL)
  {
    void *(*code)(void *, void *) = iu->iu_jit_mem + UIMM32(0);
    I = do_jit_call(rf, iu->iu_mem, code);
    NEXT(0);
  }

  VMOP(RET_R8)
    *(uint32_t *)ret = R8(0);
    return 0;

  VMOP(RET_R16)
    *(uint16_t *)ret = R16(0);
    return 0;

  VMOP(RET_R32)
    *(uint32_t *)ret = R32(0);
    return 0;

  VMOP(RET_R64)
    *(uint64_t *)ret = R64(0);
    return 0;

  VMOP(RET_R32C)
    *(uint32_t *)ret = UIMM32(0);
    return 0;
  VMOP(RET_R64C)
    *(uint64_t *)ret = UIMM64(0);
    return 0;

  VMOP(B)     I = (void *)I + (int16_t)I[0]; NEXT(0);
  VMOP(BCOND) I = (void *)I + (int16_t)(R32(0) ? I[1] : I[2]); NEXT(0);
  VMOP(JSR_VM)
    vm_printf(">>>>>>>>>>>>>>>>>>>\n");
    vm_printf("Calling %s\n", vm_funcname(I[0], iu));
    vm_exec(iu->iu_vm_funcs[I[0]], rf + I[1], iu, rf + I[2], allocaptr, -1);
    vm_printf("<<<<<<<<<<<<<<<<<<\n");
    NEXT(3);

  VMOP(JSR_EXT)
    vm_printf(">>>>>>>>>>>>>>>>>>>");
    vm_printf("Calling %s (internal)\n", vm_funcname(I[0], iu));
    iu->iu_ext_funcs[I[0]](rf + I[2], rf + I[1], iu);
    vm_printf("<<<<<<<<<<<<<<<<<<");
    NEXT(3);

  VMOP(JSR_R)
    vm_printf(">>>>>>>>>>>>>>>>>>>");
    vm_printf("Calling indirect %s (%d)\n", vm_funcname(R32(0), iu), R32(0));
    if(iu->iu_vm_funcs[R32(0)])
      vm_exec(iu->iu_vm_funcs[R32(0)], rf + I[1], iu, rf + I[2], allocaptr, -1);
    else if(iu->iu_ext_funcs[R32(0)])
      iu->iu_ext_funcs[R32(0)](rf + I[2], rf + I[1], iu);
    else
      vm_stop(iu, VM_STOP_BAD_FUNCTION, R32(0));
    vm_printf("<<<<<<<<<<<<<<<<<<");
    NEXT(3);


  VMOP(ADD_R8)  AR8(0,  R8(1) +  R8(2)); NEXT(3);
  VMOP(SUB_R8)  AR8(0,  R8(1) -  R8(2)); NEXT(3);
  VMOP(MUL_R8)  AR8(0,  R8(1) *  R8(2)); NEXT(3);
  VMOP(UDIV_R8) AR8(0,  R8(1) /  R8(2)); NEXT(3);
  VMOP(SDIV_R8) AS8(0,  S8(1) /  S8(2)); NEXT(3);
  VMOP(UREM_R8) AR8(0,  R8(1) %  R8(2)); NEXT(3);
  VMOP(SREM_R8) AS8(0,  S8(1) %  S8(2)); NEXT(3);
  VMOP(SHL_R8)  AR8(0,  R8(1) << R8(2)); NEXT(3);
  VMOP(LSHR_R8) AR8(0,  R8(1) >> R8(2)); NEXT(3);
  VMOP(ASHR_R8) AS8(0,  S8(1) >> R8(2)); NEXT(3);
  VMOP(AND_R8)  AR8(0,  R8(1) &  R8(2)); NEXT(3);
  VMOP(OR_R8)   AR8(0,  R8(1) |  R8(2)); NEXT(3);
  VMOP(XOR_R8)  AR8(0,  R8(1) ^  R8(2)); NEXT(3);

  VMOP(ADD_R8C)  AR8(0, R8(1) +  UIMM8(2)); NEXT(3);
  VMOP(SUB_R8C)  AR8(0, R8(1) -  UIMM8(2)); NEXT(3);
  VMOP(MUL_R8C)  AR8(0, R8(1) *  UIMM8(2)); NEXT(3);
  VMOP(UDIV_R8C) AR8(0, R8(1) /  UIMM8(2)); NEXT(3);
  VMOP(SDIV_R8C) AS8(0, S8(1) /  SIMM8(2)); NEXT(3);
  VMOP(UREM_R8C) AR8(0, R8(1) %  UIMM8(2)); NEXT(3);
  VMOP(SREM_R8C) AS8(0, S8(1) %  SIMM8(2)); NEXT(3);
  VMOP(SHL_R8C)  AR8(0, R8(1) << UIMM8(2)); NEXT(3);
  VMOP(LSHR_R8C) AR8(0, R8(1) >> UIMM8(2)); NEXT(3);
  VMOP(ASHR_R8C) AS8(0, S8(1) >> UIMM8(2)); NEXT(3);
  VMOP(AND_R8C)  AR8(0, R8(1) &  UIMM8(2)); NEXT(3);
  VMOP(OR_R8C)   AR8(0, R8(1) |  UIMM8(2)); NEXT(3);
  VMOP(XOR_R8C)  AR8(0, R8(1) ^  UIMM8(2)); NEXT(3);



  VMOP(ADD_R16)  AR16(0, R16(1) +  R16(2)); NEXT(3);
  VMOP(SUB_R16)  AR16(0, R16(1) -  R16(2)); NEXT(3);
  VMOP(MUL_R16)  AR16(0, R16(1) *  R16(2)); NEXT(3);
  VMOP(UDIV_R16) AR16(0, R16(1) /  R16(2)); NEXT(3);
  VMOP(SDIV_R16) AS16(0, S16(1) /  S16(2)); NEXT(3);
  VMOP(UREM_R16) AR16(0, R16(1) %  R16(2)); NEXT(3);
  VMOP(SREM_R16) AS16(0, S16(1) %  S16(2)); NEXT(3);
  VMOP(SHL_R16)  AR16(0, R16(1) << R16(2)); NEXT(3);
  VMOP(LSHR_R16) AR16(0, R16(1) >> R16(2)); NEXT(3);
  VMOP(ASHR_R16) AS16(0, S16(1) >> R16(2)); NEXT(3);
  VMOP(AND_R16)  AR16(0, R16(1) &  R16(2)); NEXT(3);
  VMOP(OR_R16)   AR16(0, R16(1) |  R16(2)); NEXT(3);
  VMOP(XOR_R16)  AR16(0, R16(1) ^  R16(2)); NEXT(3);

  VMOP(ADD_R16C)  AR16(0, R16(1) +  UIMM16(2)); NEXT(3);
  VMOP(SUB_R16C)  AR16(0, R16(1) -  UIMM16(2)); NEXT(3);
  VMOP(MUL_R16C)  AR16(0, R16(1) *  UIMM16(2)); NEXT(3);
  VMOP(UDIV_R16C) AR16(0, R16(1) /  UIMM16(2)); NEXT(3);
  VMOP(SDIV_R16C) AS16(0, S16(1) /  SIMM16(2)); NEXT(3);
  VMOP(UREM_R16C) AR16(0, R16(1) %  UIMM16(2)); NEXT(3);
  VMOP(SREM_R16C) AS16(0, S16(1) %  SIMM16(2)); NEXT(3);
  VMOP(SHL_R16C)  AR16(0, R16(1) << UIMM16(2)); NEXT(3);
  VMOP(LSHR_R16C) AR16(0, R16(1) >> UIMM16(2)); NEXT(3);
  VMOP(ASHR_R16C) AS16(0, S16(1) >> UIMM16(2)); NEXT(3);
  VMOP(AND_R16C)  AR16(0, R16(1) &  UIMM16(2)); NEXT(3);
  VMOP(OR_R16C)   AR16(0, R16(1) |  UIMM16(2)); NEXT(3);
  VMOP(XOR_R16C)  AR16(0, R16(1) ^  UIMM16(2)); NEXT(3);





  VMOP(ADD_ACC_R32)  AR32(0, R32_ACC +  R32(1)); NEXT(2);
  VMOP(SUB_ACC_R32)  AR32(0, R32_ACC -  R32(1)); NEXT(2);
  VMOP(MUL_ACC_R32)  AR32(0, R32_ACC *  R32(1)); NEXT(2);
  VMOP(UDIV_ACC_R32) AR32(0, R32_ACC /  R32(1)); NEXT(2);
  VMOP(SDIV_ACC_R32) AS32(0, S32_ACC /  S32(1)); NEXT(2);
  VMOP(UREM_ACC_R32) AR32(0, R32_ACC %  R32(1)); NEXT(2);
  VMOP(SREM_ACC_R32) AS32(0, S32_ACC %  S32(1)); NEXT(2);
  VMOP(SHL_ACC_R32)  AR32(0, R32_ACC << R32(1)); NEXT(2);
  VMOP(LSHR_ACC_R32) AR32(0, R32_ACC >> R32(1)); NEXT(2);
  VMOP(ASHR_ACC_R32) AS32(0, S32_ACC >> R32(1)); NEXT(2);
  VMOP(AND_ACC_R32)  AR32(0, R32_ACC &  R32(1)); NEXT(2);
  VMOP(OR_ACC_R32)   AR32(0, R32_ACC |  R32(1)); NEXT(2);
  VMOP(XOR_ACC_R32)  AR32(0, R32_ACC ^  R32(1)); NEXT(2);

  VMOP(INC_ACC_R32)  AR32(0, R32_ACC + 1); NEXT(1);
  VMOP(DEC_ACC_R32)  AR32(0, R32_ACC - 1); NEXT(1);

  VMOP(ADD_ACC_R32C)  AR32(0, R32_ACC +  UIMM32(1)); NEXT(3);
  VMOP(SUB_ACC_R32C)  AR32(0, R32_ACC -  UIMM32(1)); NEXT(3);
  VMOP(MUL_ACC_R32C)  AR32(0, R32_ACC *  UIMM32(1)); NEXT(3);
  VMOP(UDIV_ACC_R32C) AR32(0, R32_ACC /  UIMM32(1)); NEXT(3);
  VMOP(SDIV_ACC_R32C) AS32(0, S32_ACC /  SIMM32(1)); NEXT(3);
  VMOP(UREM_ACC_R32C) AR32(0, R32_ACC %  UIMM32(1)); NEXT(3);
  VMOP(SREM_ACC_R32C) AS32(0, S32_ACC %  SIMM32(1)); NEXT(3);
  VMOP(SHL_ACC_R32C)  AR32(0, R32_ACC << UIMM32(1)); NEXT(3);
  VMOP(LSHR_ACC_R32C) AR32(0, R32_ACC >> UIMM32(1)); NEXT(3);
  VMOP(ASHR_ACC_R32C) AS32(0, S32_ACC >> UIMM32(1)); NEXT(3);
  VMOP(AND_ACC_R32C)  AR32(0, R32_ACC &  UIMM32(1)); NEXT(3);
  VMOP(OR_ACC_R32C)   AR32(0, R32_ACC |  UIMM32(1)); NEXT(3);
  VMOP(XOR_ACC_R32C)  AR32(0, R32_ACC ^  UIMM32(1)); NEXT(3);

  VMOP(ADD_2ACC_R32)  AR32_ACC(R32_ACC +  R32(0)); NEXT(1);
  VMOP(SUB_2ACC_R32)  AR32_ACC(R32_ACC -  R32(0)); NEXT(1);
  VMOP(MUL_2ACC_R32)  AR32_ACC(R32_ACC *  R32(0)); NEXT(1);
  VMOP(UDIV_2ACC_R32) AR32_ACC(R32_ACC /  R32(0)); NEXT(1);
  VMOP(SDIV_2ACC_R32) AS32_ACC(S32_ACC /  S32(0)); NEXT(1);
  VMOP(UREM_2ACC_R32) AR32_ACC(R32_ACC %  R32(0)); NEXT(1);
  VMOP(SREM_2ACC_R32) AS32_ACC(S32_ACC %  S32(0)); NEXT(1);
  VMOP(SHL_2ACC_R32)  AR32_ACC(R32_ACC << R32(0)); NEXT(1);
  VMOP(LSHR_2ACC_R32) AR32_ACC(R32_ACC >> R32(0)); NEXT(1);
  VMOP(ASHR_2ACC_R32) AS32_ACC(S32_ACC >> R32(0)); NEXT(1);
  VMOP(AND_2ACC_R32)  AR32_ACC(R32_ACC &  R32(0)); NEXT(1);
  VMOP(OR_2ACC_R32)   AR32_ACC(R32_ACC |  R32(0)); NEXT(1);
  VMOP(XOR_2ACC_R32)  AR32_ACC(R32_ACC ^  R32(0)); NEXT(1);

  VMOP(INC_2ACC_R32)  AR32_ACC(R32_ACC + 1); NEXT(0);
  VMOP(DEC_2ACC_R32)  AR32_ACC(R32_ACC - 1); NEXT(0);

  VMOP(ADD_2ACC_R32C)  AR32_ACC(R32_ACC +  UIMM32(0)); NEXT(2);
  VMOP(SUB_2ACC_R32C)  AR32_ACC(R32_ACC -  UIMM32(0)); NEXT(2);
  VMOP(MUL_2ACC_R32C)  AR32_ACC(R32_ACC *  UIMM32(0)); NEXT(2);
  VMOP(UDIV_2ACC_R32C) AR32_ACC(R32_ACC /  UIMM32(0)); NEXT(2);
  VMOP(SDIV_2ACC_R32C) AS32_ACC(S32_ACC /  SIMM32(0)); NEXT(2);
  VMOP(UREM_2ACC_R32C) AR32_ACC(R32_ACC %  UIMM32(0)); NEXT(2);
  VMOP(SREM_2ACC_R32C) AS32_ACC(S32_ACC %  SIMM32(0)); NEXT(2);
  VMOP(SHL_2ACC_R32C)  AR32_ACC(R32_ACC << UIMM32(0)); NEXT(2);
  VMOP(LSHR_2ACC_R32C) AR32_ACC(R32_ACC >> UIMM32(0)); NEXT(2);
  VMOP(ASHR_2ACC_R32C) AS32_ACC(S32_ACC >> UIMM32(0)); NEXT(2);
  VMOP(AND_2ACC_R32C)  AR32_ACC(R32_ACC &  UIMM32(0)); NEXT(2);
  VMOP(OR_2ACC_R32C)   AR32_ACC(R32_ACC |  UIMM32(0)); NEXT(2);
  VMOP(XOR_2ACC_R32C)  AR32_ACC(R32_ACC ^  UIMM32(0)); NEXT(2);


  VMOP(ADD_R32)  AR32(0, R32(1) +  R32(2)); NEXT(3);
  VMOP(SUB_R32)  AR32(0, R32(1) -  R32(2)); NEXT(3);
  VMOP(MUL_R32)  AR32(0, R32(1) *  R32(2)); NEXT(3);
  VMOP(UDIV_R32) AR32(0, R32(1) /  R32(2)); NEXT(3);
  VMOP(SDIV_R32) AS32(0, S32(1) /  S32(2)); NEXT(3);
  VMOP(UREM_R32) AR32(0, R32(1) %  R32(2)); NEXT(3);
  VMOP(SREM_R32) AS32(0, S32(1) %  S32(2)); NEXT(3);
  VMOP(SHL_R32)  AR32(0, R32(1) << R32(2)); NEXT(3);
  VMOP(LSHR_R32) AR32(0, R32(1) >> R32(2)); NEXT(3);
  VMOP(ASHR_R32) AS32(0, S32(1) >> R32(2)); NEXT(3);
  VMOP(AND_R32)  AR32(0, R32(1) &  R32(2)); NEXT(3);
  VMOP(OR_R32)   AR32(0, R32(1) |  R32(2)); NEXT(3);
  VMOP(XOR_R32)  AR32(0, R32(1) ^  R32(2)); NEXT(3);

  VMOP(INC_R32)  AR32(0, R32(1) + 1); NEXT(2);
  VMOP(DEC_R32)  AR32(0, R32(1) - 1); NEXT(2);

  VMOP(ADD_R32C)  AR32(0, R32(1) +  UIMM32(2)); NEXT(4);
  VMOP(SUB_R32C)  AR32(0, R32(1) -  UIMM32(2)); NEXT(4);
  VMOP(MUL_R32C)  AR32(0, R32(1) *  UIMM32(2)); NEXT(4);
  VMOP(UDIV_R32C) AR32(0, R32(1) /  UIMM32(2)); NEXT(4);
  VMOP(SDIV_R32C) AS32(0, S32(1) /  SIMM32(2)); NEXT(4);
  VMOP(UREM_R32C) AR32(0, R32(1) %  UIMM32(2)); NEXT(4);
  VMOP(SREM_R32C) AS32(0, S32(1) %  SIMM32(2)); NEXT(4);
  VMOP(SHL_R32C)  AR32(0, R32(1) << UIMM32(2)); NEXT(4);
  VMOP(LSHR_R32C) AR32(0, R32(1) >> UIMM32(2)); NEXT(4);
  VMOP(ASHR_R32C) AS32(0, S32(1) >> UIMM32(2)); NEXT(4);
  VMOP(AND_R32C)  AR32(0, R32(1) &  UIMM32(2)); NEXT(4);
  VMOP(OR_R32C)   AR32(0, R32(1) |  UIMM32(2)); NEXT(4);
  VMOP(XOR_R32C)  AR32(0, R32(1) ^  UIMM32(2)); NEXT(4);




  VMOP(ADD_R64)  AR64(0, R64(1) +  R64(2)); NEXT(3);
  VMOP(SUB_R64)  AR64(0, R64(1) -  R64(2)); NEXT(3);
  VMOP(MUL_R64)  AR64(0, R64(1) *  R64(2)); NEXT(3);
  VMOP(UDIV_R64) AR64(0, R64(1) /  R64(2)); NEXT(3);
  VMOP(SDIV_R64) AS64(0, S64(1) /  S64(2)); NEXT(3);
  VMOP(UREM_R64) AR64(0, R64(1) %  R64(2)); NEXT(3);
  VMOP(SREM_R64) AS64(0, S64(1) %  S64(2)); NEXT(3);
  VMOP(SHL_R64)  AR64(0, R64(1) << R64(2)); NEXT(3);
  VMOP(LSHR_R64) AR64(0, R64(1) >> R64(2)); NEXT(3);
  VMOP(ASHR_R64) AS64(0, S64(1) >> R64(2)); NEXT(3);
  VMOP(AND_R64)  AR64(0, R64(1) &  R64(2)); NEXT(3);
  VMOP(OR_R64)   AR64(0, R64(1) |  R64(2)); NEXT(3);
  VMOP(XOR_R64)  AR64(0, R64(1) ^  R64(2)); NEXT(3);

  VMOP(ADD_R64C)  AR64(0, R64(1) +  UIMM64(2)); NEXT(6);
  VMOP(SUB_R64C)  AR64(0, R64(1) -  UIMM64(2)); NEXT(6);
  VMOP(MUL_R64C)  AR64(0, R64(1) *  UIMM64(2)); NEXT(6);
  VMOP(UDIV_R64C) AR64(0, R64(1) /  UIMM64(2)); NEXT(6);
  VMOP(SDIV_R64C) AS64(0, S64(1) /  SIMM64(2)); NEXT(6);
  VMOP(UREM_R64C) AR64(0, R64(1) %  UIMM64(2)); NEXT(6);
  VMOP(SREM_R64C) AS64(0, S64(1) %  SIMM64(2)); NEXT(6);
  VMOP(SHL_R64C)  AR64(0, R64(1) << UIMM64(2)); NEXT(6);
  VMOP(LSHR_R64C) AR64(0, R64(1) >> UIMM64(2)); NEXT(6);
  VMOP(ASHR_R64C) AS64(0, S64(1) >> UIMM64(2)); NEXT(6);
  VMOP(AND_R64C)  AR64(0, R64(1) &  UIMM64(2)); NEXT(6);
  VMOP(OR_R64C)   AR64(0, R64(1) |  UIMM64(2)); NEXT(6);
  VMOP(XOR_R64C)  AR64(0, R64(1) ^  UIMM64(2)); NEXT(6);

  VMOP(MLA32)     AR32(0, R32(1) * R32(2) + R32(3)); NEXT(4);

  VMOP(ADD_DBL) ADBL(0, RDBL(1) +  RDBL(2)); NEXT(3);
  VMOP(SUB_DBL) ADBL(0, RDBL(1) -  RDBL(2)); NEXT(3);
  VMOP(MUL_DBL) ADBL(0, RDBL(1) *  RDBL(2)); NEXT(3);
  VMOP(DIV_DBL) ADBL(0, RDBL(1) /  RDBL(2)); NEXT(3);

  VMOP(ADD_DBLC) ADBL(0, RDBL(1) +  IMMDBL(2)); NEXT(6);
  VMOP(SUB_DBLC) ADBL(0, RDBL(1) -  IMMDBL(2)); NEXT(6);
  VMOP(MUL_DBLC) ADBL(0, RDBL(1) *  IMMDBL(2)); NEXT(6);
  VMOP(DIV_DBLC) ADBL(0, RDBL(1) /  IMMDBL(2)); NEXT(6);

  VMOP(ADD_FLT) AFLT(0, RFLT(1) +  RFLT(2)); NEXT(3);
  VMOP(SUB_FLT) AFLT(0, RFLT(1) -  RFLT(2)); NEXT(3);
  VMOP(MUL_FLT) AFLT(0, RFLT(1) *  RFLT(2)); NEXT(3);
  VMOP(DIV_FLT) AFLT(0, RFLT(1) /  RFLT(2)); NEXT(3);

  VMOP(ADD_FLTC) AFLT(0, RFLT(1) +  IMMFLT(2)); NEXT(4);
  VMOP(SUB_FLTC) AFLT(0, RFLT(1) -  IMMFLT(2)); NEXT(4);
  VMOP(MUL_FLTC) AFLT(0, RFLT(1) *  IMMFLT(2)); NEXT(4);
  VMOP(DIV_FLTC) AFLT(0, RFLT(1) /  IMMFLT(2)); NEXT(4);

    // Integer compare

  VMOP(EQ8)    AR32(0, R8(1) == R8(2)); NEXT(3);
  VMOP(NE8)    AR32(0, R8(1) != R8(2)); NEXT(3);
  VMOP(UGT8)   AR32(0, R8(1) >  R8(2)); NEXT(3);
  VMOP(UGE8)   AR32(0, R8(1) >= R8(2)); NEXT(3);
  VMOP(ULT8)   AR32(0, R8(1) <  R8(2)); NEXT(3);
  VMOP(ULE8)   AR32(0, R8(1) <= R8(2)); NEXT(3);
  VMOP(SGT8)   AR32(0, S8(1) >  S8(2)); NEXT(3);
  VMOP(SGE8)   AR32(0, S8(1) >= S8(2)); NEXT(3);
  VMOP(SLT8)   AR32(0, S8(1) <  S8(2)); NEXT(3);
  VMOP(SLE8)   AR32(0, S8(1) <= S8(2)); NEXT(3);

  VMOP(EQ8_C)  AR32(0, R8(1) == UIMM8(2)); NEXT(3);
  VMOP(NE8_C)  AR32(0, R8(1) != UIMM8(2)); NEXT(3);
  VMOP(UGT8_C) AR32(0, R8(1) >  UIMM8(2)); NEXT(3);
  VMOP(UGE8_C) AR32(0, R8(1) >= UIMM8(2)); NEXT(3);
  VMOP(ULT8_C) AR32(0, R8(1) <  UIMM8(2)); NEXT(3);
  VMOP(ULE8_C) AR32(0, R8(1) <= UIMM8(2)); NEXT(3);
  VMOP(SGT8_C) AR32(0, S8(1) >  SIMM8(2)); NEXT(3);
  VMOP(SGE8_C) AR32(0, S8(1) >= SIMM8(2)); NEXT(3);
  VMOP(SLT8_C) AR32(0, S8(1) <  SIMM8(2)); NEXT(3);
  VMOP(SLE8_C) AR32(0, S8(1) <= SIMM8(2)); NEXT(3);

  VMOP(EQ16)    AR32(0, R16(1) == R16(2)); NEXT(3);
  VMOP(NE16)    AR32(0, R16(1) != R16(2)); NEXT(3);
  VMOP(UGT16)   AR32(0, R16(1) >  R16(2)); NEXT(3);
  VMOP(UGE16)   AR32(0, R16(1) >= R16(2)); NEXT(3);
  VMOP(ULT16)   AR32(0, R16(1) <  R16(2)); NEXT(3);
  VMOP(ULE16)   AR32(0, R16(1) <= R16(2)); NEXT(3);
  VMOP(SGT16)   AR32(0, S16(1) >  S16(2)); NEXT(3);
  VMOP(SGE16)   AR32(0, S16(1) >= S16(2)); NEXT(3);
  VMOP(SLT16)   AR32(0, S16(1) <  S16(2)); NEXT(3);
  VMOP(SLE16)   AR32(0, S16(1) <= S16(2)); NEXT(3);

  VMOP(EQ16_C)  AR32(0, R16(1) == UIMM16(2)); NEXT(3);
  VMOP(NE16_C)  AR32(0, R16(1) != UIMM16(2)); NEXT(3);
  VMOP(UGT16_C) AR32(0, R16(1) >  UIMM16(2)); NEXT(3);
  VMOP(UGE16_C) AR32(0, R16(1) >= UIMM16(2)); NEXT(3);
  VMOP(ULT16_C) AR32(0, R16(1) <  UIMM16(2)); NEXT(3);
  VMOP(ULE16_C) AR32(0, R16(1) <= UIMM16(2)); NEXT(3);
  VMOP(SGT16_C) AR32(0, S16(1) >  SIMM16(2)); NEXT(3);
  VMOP(SGE16_C) AR32(0, S16(1) >= SIMM16(2)); NEXT(3);
  VMOP(SLT16_C) AR32(0, S16(1) <  SIMM16(2)); NEXT(3);
  VMOP(SLE16_C) AR32(0, S16(1) <= SIMM16(2)); NEXT(3);

  VMOP(EQ32)    AR32(0, R32(1) == R32(2)); NEXT(3);
  VMOP(NE32)    AR32(0, R32(1) != R32(2)); NEXT(3);
  VMOP(UGT32)   AR32(0, R32(1) >  R32(2)); NEXT(3);
  VMOP(UGE32)   AR32(0, R32(1) >= R32(2)); NEXT(3);
  VMOP(ULT32)   AR32(0, R32(1) <  R32(2)); NEXT(3);
  VMOP(ULE32)   AR32(0, R32(1) <= R32(2)); NEXT(3);
  VMOP(SGT32)   AR32(0, S32(1) >  S32(2)); NEXT(3);
  VMOP(SGE32)   AR32(0, S32(1) >= S32(2)); NEXT(3);
  VMOP(SLT32)   AR32(0, S32(1) <  S32(2)); NEXT(3);
  VMOP(SLE32)   AR32(0, S32(1) <= S32(2)); NEXT(3);

  VMOP(EQ32_C)  AR32(0, R32(1) == UIMM32(2)); NEXT(4);
  VMOP(NE32_C)  AR32(0, R32(1) != UIMM32(2)); NEXT(4);
  VMOP(UGT32_C) AR32(0, R32(1) >  UIMM32(2)); NEXT(4);
  VMOP(UGE32_C) AR32(0, R32(1) >= UIMM32(2)); NEXT(4);
  VMOP(ULT32_C) AR32(0, R32(1) <  UIMM32(2)); NEXT(4);
  VMOP(ULE32_C) AR32(0, R32(1) <= UIMM32(2)); NEXT(4);
  VMOP(SGT32_C) AR32(0, S32(1) >  SIMM32(2)); NEXT(4);
  VMOP(SGE32_C) AR32(0, S32(1) >= SIMM32(2)); NEXT(4);
  VMOP(SLT32_C) AR32(0, S32(1) <  SIMM32(2)); NEXT(4);
  VMOP(SLE32_C) AR32(0, S32(1) <= SIMM32(2)); NEXT(4);

  VMOP(EQ64)    AR32(0, R64(1) == R64(2)); NEXT(3);
  VMOP(NE64)    AR32(0, R64(1) != R64(2)); NEXT(3);
  VMOP(UGT64)   AR32(0, R64(1) >  R64(2)); NEXT(3);
  VMOP(UGE64)   AR32(0, R64(1) >= R64(2)); NEXT(3);
  VMOP(ULT64)   AR32(0, R64(1) <  R64(2)); NEXT(3);
  VMOP(ULE64)   AR32(0, R64(1) <= R64(2)); NEXT(3);
  VMOP(SGT64)   AR32(0, S64(1) >  S64(2)); NEXT(3);
  VMOP(SGE64)   AR32(0, S64(1) >= S64(2)); NEXT(3);
  VMOP(SLT64)   AR32(0, S64(1) <  S64(2)); NEXT(3);
  VMOP(SLE64)   AR32(0, S64(1) <= S64(2)); NEXT(3);

  VMOP(EQ64_C)  AR32(0, R64(1) == UIMM64(2)); NEXT(6);
  VMOP(NE64_C)  AR32(0, R64(1) != UIMM64(2)); NEXT(6);
  VMOP(UGT64_C) AR32(0, R64(1) >  UIMM64(2)); NEXT(6);
  VMOP(UGE64_C) AR32(0, R64(1) >= UIMM64(2)); NEXT(6);
  VMOP(ULT64_C) AR32(0, R64(1) <  UIMM64(2)); NEXT(6);
  VMOP(ULE64_C) AR32(0, R64(1) <= UIMM64(2)); NEXT(6);
  VMOP(SGT64_C) AR32(0, S64(1) >  SIMM64(2)); NEXT(6);
  VMOP(SGE64_C) AR32(0, S64(1) >= SIMM64(2)); NEXT(6);
  VMOP(SLT64_C) AR32(0, S64(1) <  SIMM64(2)); NEXT(6);
  VMOP(SLE64_C) AR32(0, S64(1) <= SIMM64(2)); NEXT(6);


  VMOP(OEQ_DBL) AR32(0, RDBL(1) == RDBL(2)); NEXT(3);
  VMOP(OGT_DBL) AR32(0, RDBL(1) >  RDBL(2)); NEXT(3);
  VMOP(OGE_DBL) AR32(0, RDBL(1) >= RDBL(2)); NEXT(3);
  VMOP(OLT_DBL) AR32(0, RDBL(1) <  RDBL(2)); NEXT(3);
  VMOP(OLE_DBL) AR32(0, RDBL(1) <= RDBL(2)); NEXT(3);

  VMOP(ONE_DBL) AR32(0, !__builtin_isnan(RDBL(1))
                      && !__builtin_isnan(RDBL(2))
                      && RDBL(1) != RDBL(2)); NEXT(3);

  VMOP(ORD_DBL) AR32(0, !__builtin_isnan(RDBL(1))
                      && !__builtin_isnan(RDBL(2))); NEXT(3);

  VMOP(UNO_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(1))); NEXT(3);

  VMOP(UEQ_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(2))
                      || RDBL(1) == RDBL(2)); NEXT(3);

  VMOP(UGT_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(2))
                      || RDBL(1) >  RDBL(2)); NEXT(3);

  VMOP(UGE_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(2))
                      || RDBL(1) <= RDBL(2)); NEXT(3);

  VMOP(ULT_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(2))
                      || RDBL(1) <  RDBL(2)); NEXT(3);

  VMOP(ULE_DBL) AR32(0, __builtin_isnan(RDBL(1))
                      || __builtin_isnan(RDBL(2))
                      || RDBL(1) <= RDBL(2)); NEXT(3);

  VMOP(UNE_DBL) AR32(0, RDBL(1) != RDBL(2)); NEXT(3);



  VMOP(OEQ_DBL_C) AR32(0, RDBL(1) == IMMDBL(2)); NEXT(6);
  VMOP(OGT_DBL_C) AR32(0, RDBL(1) >  IMMDBL(2)); NEXT(6);
  VMOP(OGE_DBL_C) AR32(0, RDBL(1) >= IMMDBL(2)); NEXT(6);
  VMOP(OLT_DBL_C) AR32(0, RDBL(1) <  IMMDBL(2)); NEXT(6);
  VMOP(OLE_DBL_C) AR32(0, RDBL(1) <= IMMDBL(2)); NEXT(6);

  VMOP(ONE_DBL_C) AR32(0, !__builtin_isnan(RDBL(1)) &&
                        RDBL(1) != IMMDBL(2)); NEXT(6);
  VMOP(ORD_DBL_C) AR32(0, !__builtin_isnan(RDBL(1))); NEXT(6);
  VMOP(UNO_DBL_C) AR32(0,  __builtin_isnan(RDBL(1))); NEXT(6);
  VMOP(UEQ_DBL_C) AR32(0, __builtin_isnan(RDBL(1)) ||
                        RDBL(1) == IMMDBL(2)); NEXT(6);
  VMOP(UGT_DBL_C) AR32(0, __builtin_isnan(RDBL(1)) ||
                        RDBL(1) >  IMMDBL(2)); NEXT(6);
  VMOP(UGE_DBL_C) AR32(0, __builtin_isnan(RDBL(1)) ||
                        RDBL(1) <= IMMDBL(2)); NEXT(6);
  VMOP(ULT_DBL_C) AR32(0, __builtin_isnan(RDBL(1)) ||
                        RDBL(1) <  IMMDBL(2)); NEXT(6);
  VMOP(ULE_DBL_C) AR32(0, __builtin_isnan(RDBL(1)) ||
                        RDBL(1) <= IMMDBL(2)); NEXT(6);
  VMOP(UNE_DBL_C) AR32(0, RDBL(1) != IMMDBL(2)); NEXT(6);



  VMOP(OEQ_FLT) AR32(0, RFLT(1) == RFLT(2)); NEXT(3);
  VMOP(OGT_FLT) AR32(0, RFLT(1) >  RFLT(2)); NEXT(3);
  VMOP(OGE_FLT) AR32(0, RFLT(1) >= RFLT(2)); NEXT(3);
  VMOP(OLT_FLT) AR32(0, RFLT(1) <  RFLT(2)); NEXT(3);
  VMOP(OLE_FLT) AR32(0, RFLT(1) <= RFLT(2)); NEXT(3);

  VMOP(ONE_FLT) AR32(0, !__builtin_isnan(RFLT(1))
                      && !__builtin_isnan(RFLT(2))
                      && RFLT(1) != RFLT(2)); NEXT(3);

  VMOP(ORD_FLT) AR32(0, !__builtin_isnan(RFLT(1))
                      && !__builtin_isnan(RFLT(2))); NEXT(3);

  VMOP(UNO_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(1))); NEXT(3);

  VMOP(UEQ_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(2))
                      || RFLT(1) == RFLT(2)); NEXT(3);

  VMOP(UGT_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(2))
                      || RFLT(1) >  RFLT(2)); NEXT(3);

  VMOP(UGE_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(2))
                      || RFLT(1) <= RFLT(2)); NEXT(3);

  VMOP(ULT_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(2))
                      || RFLT(1) <  RFLT(2)); NEXT(3);

  VMOP(ULE_FLT) AR32(0, __builtin_isnan(RFLT(1))
                      || __builtin_isnan(RFLT(2))
                      || RFLT(1) <= RFLT(2)); NEXT(3);

  VMOP(UNE_FLT) AR32(0, RFLT(1) != RFLT(2)); NEXT(3);



  VMOP(OEQ_FLT_C) AR32(0, RFLT(1) == IMMFLT(2)); NEXT(4);
  VMOP(OGT_FLT_C) AR32(0, RFLT(1) >  IMMFLT(2)); NEXT(4);
  VMOP(OGE_FLT_C) AR32(0, RFLT(1) >= IMMFLT(2)); NEXT(4);
  VMOP(OLT_FLT_C) AR32(0, RFLT(1) <  IMMFLT(2)); NEXT(4);
  VMOP(OLE_FLT_C) AR32(0, RFLT(1) <= IMMFLT(2)); NEXT(4);

  VMOP(ONE_FLT_C) AR32(0, !__builtin_isnan(RFLT(1)) &&
                        RFLT(1) != IMMFLT(2)); NEXT(4);
  VMOP(ORD_FLT_C) AR32(0, !__builtin_isnan(RFLT(1))); NEXT(4);
  VMOP(UNO_FLT_C) AR32(0,  __builtin_isnan(RFLT(1))); NEXT(4);
  VMOP(UEQ_FLT_C) AR32(0, __builtin_isnan(RFLT(1)) ||
                        RFLT(1) == IMMFLT(2)); NEXT(4);
  VMOP(UGT_FLT_C) AR32(0, __builtin_isnan(RFLT(1)) ||
                        RFLT(1) >  IMMFLT(2)); NEXT(4);
  VMOP(UGE_FLT_C) AR32(0, __builtin_isnan(RFLT(1)) ||
                        RFLT(1) <= IMMFLT(2)); NEXT(4);
  VMOP(ULT_FLT_C) AR32(0, __builtin_isnan(RFLT(1)) ||
                        RFLT(1) <  IMMFLT(2)); NEXT(4);
  VMOP(ULE_FLT_C) AR32(0, __builtin_isnan(RFLT(1)) ||
                        RFLT(1) <= IMMFLT(2)); NEXT(4);
  VMOP(UNE_FLT_C) AR32(0, RFLT(1) != IMMFLT(2)); NEXT(4);


  VMOP(EQ8_BR)    I = (void *)I + (int16_t)(R8(2) == R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(NE8_BR)    I = (void *)I + (int16_t)(R8(2) != R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGT8_BR)   I = (void *)I + (int16_t)(R8(2) >  R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGE8_BR)   I = (void *)I + (int16_t)(R8(2) >= R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULT8_BR)   I = (void *)I + (int16_t)(R8(2) <  R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULE8_BR)   I = (void *)I + (int16_t)(R8(2) <= R8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGT8_BR)   I = (void *)I + (int16_t)(S8(2) >  S8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGE8_BR)   I = (void *)I + (int16_t)(S8(2) >= S8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLT8_BR)   I = (void *)I + (int16_t)(S8(2) <  S8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLE8_BR)   I = (void *)I + (int16_t)(S8(2) <= S8(3) ? I[0] : I[1]); NEXT(0);

  VMOP(EQ8_C_BR)  I = (void *)I + (int16_t)(R8(2) == UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(NE8_C_BR)  I = (void *)I + (int16_t)(R8(2) != UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGT8_C_BR) I = (void *)I + (int16_t)(R8(2) >  UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGE8_C_BR) I = (void *)I + (int16_t)(R8(2) >= UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULT8_C_BR) I = (void *)I + (int16_t)(R8(2) <  UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULE8_C_BR) I = (void *)I + (int16_t)(R8(2) <= UIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGT8_C_BR) I = (void *)I + (int16_t)(S8(2) >  SIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGE8_C_BR) I = (void *)I + (int16_t)(S8(2) >= SIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLT8_C_BR) I = (void *)I + (int16_t)(S8(2) <  SIMM8(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLE8_C_BR) I = (void *)I + (int16_t)(S8(2) <= SIMM8(3) ? I[0] : I[1]); NEXT(0);

  VMOP(EQ32_BR)    I = (void *)I + (int16_t)(R32(2) == R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(NE32_BR)    I = (void *)I + (int16_t)(R32(2) != R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGT32_BR)   I = (void *)I + (int16_t)(R32(2) >  R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGE32_BR)   I = (void *)I + (int16_t)(R32(2) >= R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULT32_BR)   I = (void *)I + (int16_t)(R32(2) <  R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULE32_BR)   I = (void *)I + (int16_t)(R32(2) <= R32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGT32_BR)   I = (void *)I + (int16_t)(S32(2) >  S32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGE32_BR)   I = (void *)I + (int16_t)(S32(2) >= S32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLT32_BR)   I = (void *)I + (int16_t)(S32(2) <  S32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLE32_BR)   I = (void *)I + (int16_t)(S32(2) <= S32(3) ? I[0] : I[1]); NEXT(0);

  VMOP(EQ32_C_BR)  I = (void *)I + (int16_t)(R32(2) == UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(NE32_C_BR)  I = (void *)I + (int16_t)(R32(2) != UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGT32_C_BR) I = (void *)I + (int16_t)(R32(2) >  UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(UGE32_C_BR) I = (void *)I + (int16_t)(R32(2) >= UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULT32_C_BR) I = (void *)I + (int16_t)(R32(2) <  UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(ULE32_C_BR) I = (void *)I + (int16_t)(R32(2) <= UIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGT32_C_BR) I = (void *)I + (int16_t)(S32(2) >  SIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SGE32_C_BR) I = (void *)I + (int16_t)(S32(2) >= SIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLT32_C_BR) I = (void *)I + (int16_t)(S32(2) <  SIMM32(3) ? I[0] : I[1]); NEXT(0);
  VMOP(SLE32_C_BR) I = (void *)I + (int16_t)(S32(2) <= SIMM32(3) ? I[0] : I[1]); NEXT(0);


  VMOP(ABS)       AR32(0, abs(S32(1)));  NEXT(2);

  VMOP(FLOOR)     ADBL(0, floor(RDBL(1)));          NEXT(2);
  VMOP(SIN)       ADBL(0,   sin(RDBL(1)));          NEXT(2);
  VMOP(COS)       ADBL(0,   cos(RDBL(1)));          NEXT(2);
  VMOP(POW)       ADBL(0,   pow(RDBL(1), RDBL(2))); NEXT(3);
  VMOP(FABS)      ADBL(0,  fabs(RDBL(1)));          NEXT(2);
  VMOP(FMOD)      ADBL(0,  fmod(RDBL(1), RDBL(2))); NEXT(3);
  VMOP(LOG10)     ADBL(0, log10(RDBL(1)));          NEXT(2);

  VMOP(FLOORF)    AFLT(0, floorf(RFLT(1)));          NEXT(2);
  VMOP(SINF)      AFLT(0,   sinf(RFLT(1)));          NEXT(2);
  VMOP(COSF)      AFLT(0,   cosf(RFLT(1)));          NEXT(2);
  VMOP(POWF)      AFLT(0,   powf(RFLT(1), RFLT(2))); NEXT(3);
  VMOP(FABSF)     AFLT(0,  fabsf(RFLT(1)));          NEXT(2);
  VMOP(FMODF)     AFLT(0,  fmodf(RFLT(1), RFLT(2))); NEXT(3);
  VMOP(LOG10F)    AFLT(0, log10f(RFLT(1)));          NEXT(2);

    // ---

  VMOP(LOAD8)        LOAD8(0, R32(1));             NEXT(2);
  VMOP(LOAD8_G)      LOAD8(0, SIMM32(1));          NEXT(3);

  VMOP(LOAD8_OFF)
    LOAD8(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD8_ZEXT_32_OFF)
    LOAD8_ZEXT_32(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD8_SEXT_32_OFF)
    LOAD8_SEXT_32(0, R32(1) + SIMM16(2));
    NEXT(3);

  VMOP(LOAD8_ROFF)
    LOAD8(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD8_ZEXT_32_ROFF)
    LOAD8_ZEXT_32(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD8_SEXT_32_ROFF)
    LOAD8_SEXT_32(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);


  VMOP(STORE8_G)     STORE8(SIMM32(1), R32(0));             NEXT(3);
  VMOP(STORE8C_OFF)  STORE8(R32(0) + SIMM16(1), SIMM8(2));  NEXT(3);
  VMOP(STORE8_OFF)   STORE8(R32(0) + SIMM16(2), R8(1));     NEXT(3);
  VMOP(STORE8)       STORE8(R32(0),             R8(1));     NEXT(2);

    // ---

  VMOP(LOAD16)       LOAD16(0, R32(1));             NEXT(2);
  VMOP(LOAD16_OFF)
    LOAD16(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD16_ZEXT_32_OFF)
    LOAD16_ZEXT_32(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD16_SEXT_32_OFF)
    LOAD16_SEXT_32(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD16_ROFF)
    LOAD16(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD16_ZEXT_32_ROFF)
    LOAD16_ZEXT_32(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD16_SEXT_32_ROFF)
    LOAD16_SEXT_32(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD16_G)     LOAD16(0, SIMM32(1));          NEXT(3);

  VMOP(STORE16_G)    STORE16(SIMM32(1), R32(0));             NEXT(3);
  VMOP(STORE16C_OFF) STORE16(R32(0) + SIMM16(1), SIMM16(2)); NEXT(3);
  VMOP(STORE16_OFF)  STORE16(R32(0) + SIMM16(2), R16(1));    NEXT(3);
  VMOP(STORE16)      STORE16(R32(0),             R16(1));    NEXT(2);

    // ---

  VMOP(LOAD32)     LOAD32(0, R32(1));                NEXT(2);
  VMOP(LOAD32_OFF)
    LOAD32(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD32_ROFF)
    LOAD32(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD32_G)   LOAD32(0, SIMM32(1));             NEXT(3);

  VMOP(STORE32_G)    STORE32(SIMM32(1), R32(0));             NEXT(3);
  VMOP(STORE32C_OFF) STORE32(R32(0) + SIMM16(1), UIMM32(2)); NEXT(4);
  VMOP(STORE32_OFF)  STORE32(R32(0) + SIMM16(2), R32(1));    NEXT(3);
  VMOP(STORE32)      STORE32(R32(0),             R32(1));    NEXT(2);

    // ---

  VMOP(LOAD64)     LOAD64(0, R32(1)            ); NEXT(2);
  VMOP(LOAD64_OFF)
    LOAD64(0, R32(1) + SIMM16(2));
    NEXT(3);
  VMOP(LOAD64_ROFF)
    LOAD64(0, R32(1) + SIMM16(2) + R32(3) * SIMM16(4));
    NEXT(5);
  VMOP(LOAD64_G)   LOAD64(0, SIMM32(1)         ); NEXT(3);

  VMOP(STORE64_G)    STORE64(SIMM32(1),          R64(0));    NEXT(3);
  VMOP(STORE64C_OFF) STORE64(R32(0) + SIMM16(1), UIMM64(2)); NEXT(6);
  VMOP(STORE64_OFF)  STORE64(R32(0) + SIMM16(2), R64(1));    NEXT(3);
  VMOP(STORE64)      STORE64(R32(0),             R64(1));    NEXT(2);


  VMOP(MOV8)    AR8(0,  R8(1));   NEXT(2);
  VMOP(MOV32)   AR32(0, R32(1));   NEXT(2);
  VMOP(MOV64)   AR64(0, R64(1));   NEXT(2);

  VMOP(MOV8_C)  AR8(0,  UIMM8(1)); NEXT(2);
  VMOP(MOV16_C) AR16(0, UIMM16(1)); NEXT(2);
  VMOP(MOV32_C) AR32(0, UIMM32(1)); NEXT(3);
  VMOP(MOV64_C) AR64(0, UIMM64(1)); NEXT(5);

  VMOP(LEA_R32_SHL)     AR32(0, R32(1) + (R32(2) << I[3])); NEXT(4);
  VMOP(LEA_R32_SHL2)    AR32(0, R32(1) + (R32(2) << 2)); NEXT(3);
  VMOP(LEA_R32_SHL_OFF) AR32(0, R32(1) + (R32(2) << I[3]) + SIMM32(4)); NEXT(6);
  VMOP(LEA_R32_MUL_OFF) AR32(0, R32(1) + R32(2) * UIMM32(3) + SIMM32(5)); NEXT(7);

  VMOP(CAST_1_TRUNC_8)   AR32(0, !!R8(1)); NEXT(2);
  VMOP(CAST_1_TRUNC_16)   AR32(0, !!R16(1)); NEXT(2);

  VMOP(CAST_8_ZEXT_1)    AR8(0, R32(1)); NEXT(2);
  VMOP(CAST_8_TRUNC_16)  AR8(0, R16(1)); NEXT(2);
  VMOP(CAST_8_TRUNC_32)  AR8(0, R32(1)); NEXT(2);
  VMOP(CAST_8_TRUNC_64)  AR8(0, R64(1)); NEXT(2);

  VMOP(CAST_16_ZEXT_1)   AR16(0, R32(1)); NEXT(2);
  VMOP(CAST_16_ZEXT_8)   AR16(0, R8(1)); NEXT(2);
  VMOP(CAST_16_SEXT_8)   AS16(0, S8(1)); NEXT(2);
  VMOP(CAST_16_TRUNC_32) AR16(0, R32(1)); NEXT(2);
  VMOP(CAST_16_TRUNC_64) AR16(0, R64(1)); NEXT(2);
  VMOP(CAST_16_FPTOSI_FLT) AS16(0, RFLT(1)); NEXT(2);
  VMOP(CAST_16_FPTOUI_FLT) AR16(0, RFLT(1)); NEXT(2);
  VMOP(CAST_16_FPTOSI_DBL) AS16(0, RDBL(1)); NEXT(2);
  VMOP(CAST_16_FPTOUI_DBL) AR16(0, RDBL(1)); NEXT(2);

  VMOP(CAST_32_SEXT_1)   AR32(0, R32(1) ? -1 : 0); NEXT(2);
  VMOP(CAST_32_ZEXT_8)   AR32(0, R8(1)); NEXT(2);
  VMOP(CAST_32_SEXT_8)   AS32(0, S8(1)); NEXT(2);
  VMOP(CAST_32_ZEXT_16)  AR32(0, R16(1)); NEXT(2);
  VMOP(CAST_32_SEXT_16)  AS32(0, S16(1)); NEXT(2);
  VMOP(CAST_32_TRUNC_64) AR32(0, R64(1)); NEXT(2);

  VMOP(CAST_32_FPTOSI_FLT) AS32(0, RFLT(1)); NEXT(2);
  VMOP(CAST_32_FPTOUI_FLT) AR32(0, RFLT(1)); NEXT(2);
  VMOP(CAST_32_FPTOSI_DBL) AS32(0, RDBL(1)); NEXT(2);
  VMOP(CAST_32_FPTOUI_DBL) AR32(0, RDBL(1)); NEXT(2);


  VMOP(CAST_64_ZEXT_1) AR64(0, R32(1)); NEXT(2);
  VMOP(CAST_64_SEXT_1) AS64(0, R32(1) ? (int64_t)-1LL : 0); NEXT(2);
  VMOP(CAST_64_ZEXT_8) AR64(0, R8(1)); NEXT(2);
  VMOP(CAST_64_SEXT_8) AS64(0, S8(1)); NEXT(2);
  VMOP(CAST_64_ZEXT_16) AR64(0, R16(1)); NEXT(2);
  VMOP(CAST_64_SEXT_16) AS64(0, S16(1)); NEXT(2);
  VMOP(CAST_64_ZEXT_32) AR64(0, R32(1)); NEXT(2);
  VMOP(CAST_64_SEXT_32) AS64(0, S32(1)); NEXT(2);
  VMOP(CAST_64_FPTOSI_FLT) AS64(0, RFLT(1)); NEXT(2);
  VMOP(CAST_64_FPTOUI_FLT) AR64(0, RFLT(1)); NEXT(2);
  VMOP(CAST_64_FPTOSI_DBL) AS64(0, RDBL(1)); NEXT(2);
  VMOP(CAST_64_FPTOUI_DBL) AR64(0, RDBL(1)); NEXT(2);

  VMOP(CAST_DBL_FPEXT_FLT)   ADBL(0, RFLT(1)); NEXT(2);

  VMOP(CAST_FLT_FPTRUNC_DBL) AFLT(0, RDBL(1)); NEXT(2);
  VMOP(CAST_FLT_SITOFP_8)    AFLT(0, S8(1)); NEXT(2);
  VMOP(CAST_FLT_UITOFP_8)    AFLT(0, R8(1)); NEXT(2);
  VMOP(CAST_FLT_SITOFP_16)   AFLT(0, S16(1)); NEXT(2);
  VMOP(CAST_FLT_UITOFP_16)   AFLT(0, R16(1)); NEXT(2);
  VMOP(CAST_FLT_SITOFP_32)   AFLT(0, S32(1)); NEXT(2);
  VMOP(CAST_FLT_UITOFP_32)   AFLT(0, R32(1)); NEXT(2);
  VMOP(CAST_FLT_SITOFP_64)   AFLT(0, S64(1)); NEXT(2);
  VMOP(CAST_FLT_UITOFP_64)   AFLT(0, R64(1)); NEXT(2);

  VMOP(CAST_DBL_SITOFP_8)    ADBL(0, S8(1)); NEXT(2);
  VMOP(CAST_DBL_UITOFP_8)    ADBL(0, R8(1)); NEXT(2);
  VMOP(CAST_DBL_SITOFP_16)   ADBL(0, S16(1)); NEXT(2);
  VMOP(CAST_DBL_UITOFP_16)   ADBL(0, R16(1)); NEXT(2);
  VMOP(CAST_DBL_SITOFP_32)   ADBL(0, S32(1)); NEXT(2);
  VMOP(CAST_DBL_UITOFP_32)   ADBL(0, R32(1)); NEXT(2);
  VMOP(CAST_DBL_SITOFP_64)   ADBL(0, S64(1)); NEXT(2);
  VMOP(CAST_DBL_UITOFP_64)   ADBL(0, R64(1)); NEXT(2);

  VMOP(JUMPTABLE)
    I = (void *)I + (int16_t)I[2 + R8(0)]; NEXT(0);

  VMOP(SWITCH8_BS) {
      const uint8_t u8 = R8(0);
      const uint32_t p = I[1];
      int imin = 0;
      int imax = p - 1;
      const uint16_t *Iorg = I;
      I += 2;
      while(imin < imax) {
        int imid = (imin + imax) >> 1;

        if(UIMM8(imid) < u8)
          imin = imid + 1;
        else
          imax = imid;
      }
      if(!((imax == imin) && (UIMM8(imin) == u8)))
        imin = p;

      I = (void *)Iorg + (int16_t)I[p + imin];
      NEXT(0);
    }

  VMOP(SWITCH32_BS) {
      const uint32_t u32 = R32(0);
      const uint32_t p = I[1];
      int imin = 0;
      int imax = p - 1;
      const uint16_t *Iorg = I;
      I += 2;
      while(imin < imax) {
        int imid = (imin + imax) >> 1;

        if(UIMM32(imid * 2) < u32)
          imin = imid + 1;
        else
          imax = imid;
      }
      if(!((imax == imin) && (UIMM32(imin * 2) == u32)))
        imin = p;
      I = (void *)Iorg + (int16_t)I[p * 2 + imin];
      NEXT(0);
    }


  VMOP(SWITCH64_BS) {
      const uint64_t u64 = R64(0);
      const uint32_t p = I[1];
      int imin = 0;
      int imax = p - 1;
      const uint16_t *Iorg = I;
      I += 2;
      while(imin < imax) {
        int imid = (imin + imax) >> 1;

        if(UIMM64(imid * 4) < u64)
          imin = imid + 1;
        else
          imax = imid;
      }
      if(!((imax == imin) && (UIMM64(imin * 4) == u64)))
        imin = p;
      I = (void *)Iorg + (int16_t)I[p * 4 + imin];
      NEXT(0);
    }


  VMOP(SELECT8RR) AR8(0, R32(1) ? R8(2)    : R8(3));    NEXT(4);
  VMOP(SELECT8RC) AR8(0, R32(1) ? R8(2)    : UIMM8(3)); NEXT(4);
  VMOP(SELECT8CR) AR8(0, R32(1) ? UIMM8(3) : R8(2));    NEXT(4);
  VMOP(SELECT8CC) AR8(0, R32(1) ? UIMM8(2) : UIMM8(4)); NEXT(4);

  VMOP(SELECT16RR) AR16(0, R32(1) ? R16(2)    : R16(3));    NEXT(4);
  VMOP(SELECT16RC) AR16(0, R32(1) ? R16(2)    : UIMM16(3)); NEXT(4);
  VMOP(SELECT16CR) AR16(0, R32(1) ? UIMM16(3) : R16(2));    NEXT(4);
  VMOP(SELECT16CC) AR16(0, R32(1) ? UIMM16(2) : UIMM16(4)); NEXT(4);

  VMOP(SELECT32RR) AR32(0, R32(1) ? R32(2)    : R32(3));    NEXT(4);
  VMOP(SELECT32RC) AR32(0, R32(1) ? R32(2)    : UIMM32(3)); NEXT(5);
  VMOP(SELECT32CR) AR32(0, R32(1) ? UIMM32(3) : R32(2));    NEXT(5);
  VMOP(SELECT32CC) AR32(0, R32(1) ? UIMM32(2) : UIMM32(4)); NEXT(6);

  VMOP(SELECT64RR) AR64(0, R32(1) ? R64(2)    : R64(3));    NEXT(4);
  VMOP(SELECT64RC) AR64(0, R32(1) ? R64(2)    : UIMM64(3)); NEXT(7);
  VMOP(SELECT64CR) AR64(0, R32(1) ? UIMM64(3) : R64(2));    NEXT(7);
  VMOP(SELECT64CC) AR64(0, R32(1) ? UIMM64(2) : UIMM64(6)); NEXT(10);


  VMOP(ALLOCA) {
      allocaptr = VMIR_ALIGN(allocaptr, I[1]);
      AR32(0, allocaptr);
      allocaptr += UIMM32(2);
      NEXT(4);
    }

  VMOP(ALLOCAD) {
      allocaptr = VMIR_ALIGN(allocaptr, I[1]);
      uint32_t r = allocaptr;
      allocaptr += UIMM32(3) * R32(2);
      AR32(0, r);
      NEXT(5);
    }

  VMOP(STACKSHRINK)
    allocaptr -= UIMM32(0);
    NEXT(2);

  VMOP(STACKSAVE)
    AR32(0, allocaptr);
    NEXT(1);

  VMOP(STACKRESTORE)
    allocaptr = R32(0);
    NEXT(1);

  VMOP(STACKCOPYR)
    allocaptr = VMIR_ALIGN(allocaptr, 4);
    AR32(0, allocaptr);
    memcpy(MEM(allocaptr), MEM(R32(1)), UIMM32(2));
    allocaptr += UIMM32(2);
    NEXT(4);

  VMOP(STACKCOPYC)
    allocaptr = VMIR_ALIGN(allocaptr, 4);
    AR32(0, allocaptr);
    memcpy(MEM(allocaptr), MEM(UIMM32(1)), UIMM32(3));
    allocaptr += UIMM32(3);
    NEXT(5);

  VMOP(UNREACHABLE) vm_stop(iu, VM_STOP_UNREACHABLE, 0);


  VMOP(MEMCPY) {
      uint32_t r = R32(1);
      memcpy(MEM(R32(1)), MEM(R32(2)), R32(3));
      AR32(0, r);
      NEXT(4);
    }

  VMOP(MEMSET) {
      uint32_t r = R32(1);
      memset(MEM(R32(1)), R32(2), R32(3));
      AR32(0, r);
      NEXT(4);
    }

  VMOP(MEMMOVE) {
      uint32_t r = R32(1);
      memmove(MEM(R32(1)), MEM(R32(2)), R32(3));
      AR32(0, r);
      NEXT(4);
    }

  VMOP(LLVM_MEMCPY)
    memcpy(MEM(R32(0)), MEM(R32(1)), R32(2)); NEXT(3);
  VMOP(LLVM_MEMSET)
    memset(MEM(R32(0)), R8(1), R32(2)); NEXT(3);
  VMOP(LLVM_MEMSET64)
    memset(MEM(R32(0)), R8(1), R64(2)); NEXT(3);

  VMOP(MEMCMP)
    AR32(0, memcmp(MEM(R32(1)), MEM(R32(2)), R32(3))); NEXT(4);

  VMOP(STRCPY) {
      uint32_t r = R32(1);
      strcpy(MEM(R32(1)), MEM(R32(2)));
      AR32(0, r);
      NEXT(3);
    }

  VMOP(STRNCPY) {
      uint32_t r = R32(1);
      strncpy(MEM(R32(1)), MEM(R32(2)), R32(3));
      AR32(0, r);
      NEXT(4);
    }

  VMOP(STRCMP)
    AR32(0, strcmp(MEM(R32(1)), MEM(R32(2)))); NEXT(3);
  VMOP(STRNCMP)
    AR32(0, strncmp(MEM(R32(1)), MEM(R32(2)), R32(3))); NEXT(4);
  VMOP(STRCHR)
    AR32(0, vm_strchr(R32(1), R32(2), mem)); NEXT(3);
  VMOP(STRRCHR)
    AR32(0, vm_strrchr(R32(1), R32(2), mem)); NEXT(3);
  VMOP(STRLEN)
    AR32(0, strlen(MEM(R32(1)))); NEXT(2);

  VMOP(VAARG32)
    AR32(0, vm_vaarg32(rf, MEM(R32(1)))); NEXT(2);

  VMOP(VAARG64)
    AR64(0, vm_vaarg64(rf, MEM(R32(1)))); NEXT(2);

  VMOP(VASTART)
    *(void **)MEM(R32(0)) = rf + S32(1);
    NEXT(2);

  VMOP(VACOPY)
    *(void **)MEM(R32(0)) = *(void **)MEM(R32(1));
    NEXT(2);

  VMOP(CTZ32) AR32(0, __builtin_ctz(R32(1))); NEXT(2);
  VMOP(CLZ32) AR32(0, __builtin_clz(R32(1))); NEXT(2);
  VMOP(POP32) AR32(0, __builtin_popcount(R32(1))); NEXT(2);

  VMOP(CTZ64) AR64(0, __builtin_ctzll(R64(1))); NEXT(2);
  VMOP(CLZ64) AR64(0, __builtin_clzll(R64(1))); NEXT(2);
  VMOP(POP64) AR64(0, __builtin_popcountll(R64(1))); NEXT(2);

  VMOP(UADDO32)
  {
    uint32_t r;
#if __has_builtin(__builtin_uadd_overflow)
    AR32(1, __builtin_uadd_overflow(R32(2), R32(3), &r));
#else
    uint32_t a = R32(2);
    uint32_t b = R32(3);
    AR32(1, UINT32_MAX - a < b);
    r = a + b;
#endif
    AR32(0, r);
    NEXT(4);
  }

  VMOP(INSTRUMENT_COUNT)
#ifdef VM_TRACE
  {
    ir_instrumentation_t *ii = &VECTOR_ITEM(&iu->iu_instrumentation, UIMM32(0));
    printf("!!! BASIC BLOCK %s.%d\n", ii->ii_func->if_name, ii->ii_bb);
  }
#endif
    VECTOR_ITEM(&iu->iu_instrumentation, UIMM32(0)).ii_count++;
    NEXT(2);
  }

#ifdef VM_USE_COMPUTED_GOTO

 resolve:
  switch(op) {
  case VM_NOP:       return &&NOP      - &&opz;     break;

  case VM_JIT_CALL:  return &&JIT_CALL - &&opz;     break;
  case VM_RET_VOID:  return &&RET_VOID - &&opz;     break;
  case VM_RET_R8:    return &&RET_R8   - &&opz;     break;
  case VM_RET_R16:   return &&RET_R16  - &&opz;     break;
  case VM_RET_R32:   return &&RET_R32  - &&opz;     break;
  case VM_RET_R64:   return &&RET_R64  - &&opz;     break;
  case VM_RET_R32C:  return &&RET_R32C - &&opz;     break;
  case VM_RET_R64C:  return &&RET_R64C - &&opz;     break;

  case VM_ADD_R8:   return &&ADD_R8  - &&opz;     break;
  case VM_SUB_R8:   return &&SUB_R8  - &&opz;     break;
  case VM_MUL_R8:   return &&MUL_R8  - &&opz;     break;
  case VM_UDIV_R8:  return &&UDIV_R8 - &&opz;     break;
  case VM_SDIV_R8:  return &&SDIV_R8 - &&opz;     break;
  case VM_UREM_R8:  return &&UREM_R8 - &&opz;     break;
  case VM_SREM_R8:  return &&SREM_R8 - &&opz;     break;
  case VM_SHL_R8:   return &&SHL_R8  - &&opz;     break;
  case VM_LSHR_R8:  return &&LSHR_R8 - &&opz;     break;
  case VM_ASHR_R8:  return &&ASHR_R8 - &&opz;     break;
  case VM_AND_R8:   return &&AND_R8  - &&opz;     break;
  case VM_OR_R8:    return &&OR_R8   - &&opz;     break;
  case VM_XOR_R8:   return &&XOR_R8  - &&opz;     break;

  case VM_ADD_R8C:   return &&ADD_R8C  - &&opz;     break;
  case VM_SUB_R8C:   return &&SUB_R8C  - &&opz;     break;
  case VM_MUL_R8C:   return &&MUL_R8C  - &&opz;     break;
  case VM_UDIV_R8C:  return &&UDIV_R8C - &&opz;     break;
  case VM_SDIV_R8C:  return &&SDIV_R8C - &&opz;     break;
  case VM_UREM_R8C:  return &&UREM_R8C - &&opz;     break;
  case VM_SREM_R8C:  return &&SREM_R8C - &&opz;     break;
  case VM_SHL_R8C:   return &&SHL_R8C  - &&opz;     break;
  case VM_LSHR_R8C:  return &&LSHR_R8C - &&opz;     break;
  case VM_ASHR_R8C:  return &&ASHR_R8C - &&opz;     break;
  case VM_AND_R8C:   return &&AND_R8C  - &&opz;     break;
  case VM_OR_R8C:    return &&OR_R8C   - &&opz;     break;
  case VM_XOR_R8C:   return &&XOR_R8C  - &&opz;     break;

  case VM_ADD_R16:   return &&ADD_R16  - &&opz;     break;
  case VM_SUB_R16:   return &&SUB_R16  - &&opz;     break;
  case VM_MUL_R16:   return &&MUL_R16  - &&opz;     break;
  case VM_UDIV_R16:  return &&UDIV_R16 - &&opz;     break;
  case VM_SDIV_R16:  return &&SDIV_R16 - &&opz;     break;
  case VM_UREM_R16:  return &&UREM_R16 - &&opz;     break;
  case VM_SREM_R16:  return &&SREM_R16 - &&opz;     break;
  case VM_SHL_R16:   return &&SHL_R16  - &&opz;     break;
  case VM_LSHR_R16:  return &&LSHR_R16 - &&opz;     break;
  case VM_ASHR_R16:  return &&ASHR_R16 - &&opz;     break;
  case VM_AND_R16:   return &&AND_R16  - &&opz;     break;
  case VM_OR_R16:    return &&OR_R16   - &&opz;     break;
  case VM_XOR_R16:   return &&XOR_R16  - &&opz;     break;

  case VM_ADD_R16C:   return &&ADD_R16C  - &&opz;     break;
  case VM_SUB_R16C:   return &&SUB_R16C  - &&opz;     break;
  case VM_MUL_R16C:   return &&MUL_R16C  - &&opz;     break;
  case VM_UDIV_R16C:  return &&UDIV_R16C - &&opz;     break;
  case VM_SDIV_R16C:  return &&SDIV_R16C - &&opz;     break;
  case VM_UREM_R16C:  return &&UREM_R16C - &&opz;     break;
  case VM_SREM_R16C:  return &&SREM_R16C - &&opz;     break;
  case VM_SHL_R16C:   return &&SHL_R16C  - &&opz;     break;
  case VM_LSHR_R16C:  return &&LSHR_R16C - &&opz;     break;
  case VM_ASHR_R16C:  return &&ASHR_R16C - &&opz;     break;
  case VM_AND_R16C:   return &&AND_R16C  - &&opz;     break;
  case VM_OR_R16C:    return &&OR_R16C   - &&opz;     break;
  case VM_XOR_R16C:   return &&XOR_R16C  - &&opz;     break;

  case VM_ADD_R32:   return &&ADD_R32  - &&opz;     break;
  case VM_SUB_R32:   return &&SUB_R32  - &&opz;     break;
  case VM_MUL_R32:   return &&MUL_R32  - &&opz;     break;
  case VM_UDIV_R32:  return &&UDIV_R32 - &&opz;     break;
  case VM_SDIV_R32:  return &&SDIV_R32 - &&opz;     break;
  case VM_UREM_R32:  return &&UREM_R32 - &&opz;     break;
  case VM_SREM_R32:  return &&SREM_R32 - &&opz;     break;
  case VM_SHL_R32:   return &&SHL_R32  - &&opz;     break;
  case VM_LSHR_R32:  return &&LSHR_R32 - &&opz;     break;
  case VM_ASHR_R32:  return &&ASHR_R32 - &&opz;     break;
  case VM_AND_R32:   return &&AND_R32  - &&opz;     break;
  case VM_OR_R32:    return &&OR_R32   - &&opz;     break;
  case VM_XOR_R32:   return &&XOR_R32  - &&opz;     break;

  case VM_INC_R32:   return &&INC_R32  - &&opz;     break;
  case VM_DEC_R32:   return &&DEC_R32  - &&opz;     break;

  case VM_ADD_R32C:   return &&ADD_R32C  - &&opz;     break;
  case VM_SUB_R32C:   return &&SUB_R32C  - &&opz;     break;
  case VM_MUL_R32C:   return &&MUL_R32C  - &&opz;     break;
  case VM_UDIV_R32C:  return &&UDIV_R32C - &&opz;     break;
  case VM_SDIV_R32C:  return &&SDIV_R32C - &&opz;     break;
  case VM_UREM_R32C:  return &&UREM_R32C - &&opz;     break;
  case VM_SREM_R32C:  return &&SREM_R32C - &&opz;     break;
  case VM_SHL_R32C:   return &&SHL_R32C  - &&opz;     break;
  case VM_LSHR_R32C:  return &&LSHR_R32C - &&opz;     break;
  case VM_ASHR_R32C:  return &&ASHR_R32C - &&opz;     break;
  case VM_AND_R32C:   return &&AND_R32C  - &&opz;     break;
  case VM_OR_R32C:    return &&OR_R32C   - &&opz;     break;
  case VM_XOR_R32C:   return &&XOR_R32C  - &&opz;     break;

  case VM_ADD_ACC_R32:   return &&ADD_ACC_R32  - &&opz;     break;
  case VM_SUB_ACC_R32:   return &&SUB_ACC_R32  - &&opz;     break;
  case VM_MUL_ACC_R32:   return &&MUL_ACC_R32  - &&opz;     break;
  case VM_UDIV_ACC_R32:  return &&UDIV_ACC_R32 - &&opz;     break;
  case VM_SDIV_ACC_R32:  return &&SDIV_ACC_R32 - &&opz;     break;
  case VM_UREM_ACC_R32:  return &&UREM_ACC_R32 - &&opz;     break;
  case VM_SREM_ACC_R32:  return &&SREM_ACC_R32 - &&opz;     break;
  case VM_SHL_ACC_R32:   return &&SHL_ACC_R32  - &&opz;     break;
  case VM_LSHR_ACC_R32:  return &&LSHR_ACC_R32 - &&opz;     break;
  case VM_ASHR_ACC_R32:  return &&ASHR_ACC_R32 - &&opz;     break;
  case VM_AND_ACC_R32:   return &&AND_ACC_R32  - &&opz;     break;
  case VM_OR_ACC_R32:    return &&OR_ACC_R32   - &&opz;     break;
  case VM_XOR_ACC_R32:   return &&XOR_ACC_R32  - &&opz;     break;
  case VM_INC_ACC_R32:   return &&INC_ACC_R32  - &&opz;     break;
  case VM_DEC_ACC_R32:   return &&DEC_ACC_R32  - &&opz;     break;
  case VM_ADD_ACC_R32C:   return &&ADD_ACC_R32C  - &&opz;     break;
  case VM_SUB_ACC_R32C:   return &&SUB_ACC_R32C  - &&opz;     break;
  case VM_MUL_ACC_R32C:   return &&MUL_ACC_R32C  - &&opz;     break;
  case VM_UDIV_ACC_R32C:  return &&UDIV_ACC_R32C - &&opz;     break;
  case VM_SDIV_ACC_R32C:  return &&SDIV_ACC_R32C - &&opz;     break;
  case VM_UREM_ACC_R32C:  return &&UREM_ACC_R32C - &&opz;     break;
  case VM_SREM_ACC_R32C:  return &&SREM_ACC_R32C - &&opz;     break;
  case VM_SHL_ACC_R32C:   return &&SHL_ACC_R32C  - &&opz;     break;
  case VM_LSHR_ACC_R32C:  return &&LSHR_ACC_R32C - &&opz;     break;
  case VM_ASHR_ACC_R32C:  return &&ASHR_ACC_R32C - &&opz;     break;
  case VM_AND_ACC_R32C:   return &&AND_ACC_R32C  - &&opz;     break;
  case VM_OR_ACC_R32C:    return &&OR_ACC_R32C   - &&opz;     break;
  case VM_XOR_ACC_R32C:   return &&XOR_ACC_R32C  - &&opz;     break;

  case VM_ADD_2ACC_R32:   return &&ADD_2ACC_R32  - &&opz;     break;
  case VM_SUB_2ACC_R32:   return &&SUB_2ACC_R32  - &&opz;     break;
  case VM_MUL_2ACC_R32:   return &&MUL_2ACC_R32  - &&opz;     break;
  case VM_UDIV_2ACC_R32:  return &&UDIV_2ACC_R32 - &&opz;     break;
  case VM_SDIV_2ACC_R32:  return &&SDIV_2ACC_R32 - &&opz;     break;
  case VM_UREM_2ACC_R32:  return &&UREM_2ACC_R32 - &&opz;     break;
  case VM_SREM_2ACC_R32:  return &&SREM_2ACC_R32 - &&opz;     break;
  case VM_SHL_2ACC_R32:   return &&SHL_2ACC_R32  - &&opz;     break;
  case VM_LSHR_2ACC_R32:  return &&LSHR_2ACC_R32 - &&opz;     break;
  case VM_ASHR_2ACC_R32:  return &&ASHR_2ACC_R32 - &&opz;     break;
  case VM_AND_2ACC_R32:   return &&AND_2ACC_R32  - &&opz;     break;
  case VM_OR_2ACC_R32:    return &&OR_2ACC_R32   - &&opz;     break;
  case VM_XOR_2ACC_R32:   return &&XOR_2ACC_R32  - &&opz;     break;
  case VM_INC_2ACC_R32:   return &&INC_2ACC_R32  - &&opz;     break;
  case VM_DEC_2ACC_R32:   return &&DEC_2ACC_R32  - &&opz;     break;
  case VM_ADD_2ACC_R32C:   return &&ADD_2ACC_R32C  - &&opz;     break;
  case VM_SUB_2ACC_R32C:   return &&SUB_2ACC_R32C  - &&opz;     break;
  case VM_MUL_2ACC_R32C:   return &&MUL_2ACC_R32C  - &&opz;     break;
  case VM_UDIV_2ACC_R32C:  return &&UDIV_2ACC_R32C - &&opz;     break;
  case VM_SDIV_2ACC_R32C:  return &&SDIV_2ACC_R32C - &&opz;     break;
  case VM_UREM_2ACC_R32C:  return &&UREM_2ACC_R32C - &&opz;     break;
  case VM_SREM_2ACC_R32C:  return &&SREM_2ACC_R32C - &&opz;     break;
  case VM_SHL_2ACC_R32C:   return &&SHL_2ACC_R32C  - &&opz;     break;
  case VM_LSHR_2ACC_R32C:  return &&LSHR_2ACC_R32C - &&opz;     break;
  case VM_ASHR_2ACC_R32C:  return &&ASHR_2ACC_R32C - &&opz;     break;
  case VM_AND_2ACC_R32C:   return &&AND_2ACC_R32C  - &&opz;     break;
  case VM_OR_2ACC_R32C:    return &&OR_2ACC_R32C   - &&opz;     break;
  case VM_XOR_2ACC_R32C:   return &&XOR_2ACC_R32C  - &&opz;     break;


  case VM_ADD_R64:   return &&ADD_R64  - &&opz;     break;
  case VM_SUB_R64:   return &&SUB_R64  - &&opz;     break;
  case VM_MUL_R64:   return &&MUL_R64  - &&opz;     break;
  case VM_UDIV_R64:  return &&UDIV_R64 - &&opz;     break;
  case VM_SDIV_R64:  return &&SDIV_R64 - &&opz;     break;
  case VM_UREM_R64:  return &&UREM_R64 - &&opz;     break;
  case VM_SREM_R64:  return &&SREM_R64 - &&opz;     break;
  case VM_SHL_R64:   return &&SHL_R64  - &&opz;     break;
  case VM_LSHR_R64:  return &&LSHR_R64 - &&opz;     break;
  case VM_ASHR_R64:  return &&ASHR_R64 - &&opz;     break;
  case VM_AND_R64:   return &&AND_R64  - &&opz;     break;
  case VM_OR_R64:    return &&OR_R64   - &&opz;     break;
  case VM_XOR_R64:   return &&XOR_R64  - &&opz;     break;

  case VM_ADD_R64C:  return &&ADD_R64C  - &&opz;     break;
  case VM_SUB_R64C:  return &&SUB_R64C  - &&opz;     break;
  case VM_MUL_R64C:  return &&MUL_R64C  - &&opz;     break;
  case VM_UDIV_R64C: return &&UDIV_R64C - &&opz;     break;
  case VM_SDIV_R64C: return &&SDIV_R64C - &&opz;     break;
  case VM_UREM_R64C: return &&UREM_R64C - &&opz;     break;
  case VM_SREM_R64C: return &&SREM_R64C - &&opz;     break;
  case VM_SHL_R64C:  return &&SHL_R64C  - &&opz;     break;
  case VM_LSHR_R64C: return &&LSHR_R64C - &&opz;     break;
  case VM_ASHR_R64C: return &&ASHR_R64C - &&opz;     break;
  case VM_AND_R64C:  return &&AND_R64C  - &&opz;     break;
  case VM_OR_R64C:   return &&OR_R64C   - &&opz;     break;
  case VM_XOR_R64C:  return &&XOR_R64C  - &&opz;     break;

  case VM_MLA32:     return &&MLA32     - &&opz;     break;

  case VM_ADD_DBL:   return &&ADD_DBL  - &&opz;     break;
  case VM_SUB_DBL:   return &&SUB_DBL  - &&opz;     break;
  case VM_MUL_DBL:   return &&MUL_DBL  - &&opz;     break;
  case VM_DIV_DBL:   return &&DIV_DBL  - &&opz;     break;

  case VM_ADD_DBLC:   return &&ADD_DBLC  - &&opz;     break;
  case VM_SUB_DBLC:   return &&SUB_DBLC  - &&opz;     break;
  case VM_MUL_DBLC:   return &&MUL_DBLC  - &&opz;     break;
  case VM_DIV_DBLC:   return &&DIV_DBLC  - &&opz;     break;

  case VM_ADD_FLT:   return &&ADD_FLT  - &&opz;     break;
  case VM_SUB_FLT:   return &&SUB_FLT  - &&opz;     break;
  case VM_MUL_FLT:   return &&MUL_FLT  - &&opz;     break;
  case VM_DIV_FLT:   return &&DIV_FLT  - &&opz;     break;

  case VM_ADD_FLTC:   return &&ADD_FLTC  - &&opz;     break;
  case VM_SUB_FLTC:   return &&SUB_FLTC  - &&opz;     break;
  case VM_MUL_FLTC:   return &&MUL_FLTC  - &&opz;     break;
  case VM_DIV_FLTC:   return &&DIV_FLTC  - &&opz;     break;

  case VM_ABS: return &&ABS - &&opz; break;

  case VM_FLOOR: return &&FLOOR - &&opz; break;
  case VM_SIN: return &&SIN - &&opz; break;
  case VM_COS: return &&COS - &&opz; break;
  case VM_POW: return &&POW - &&opz; break;
  case VM_FABS: return &&FABS - &&opz; break;
  case VM_FMOD: return &&FMOD - &&opz; break;
  case VM_LOG10: return &&LOG10 - &&opz; break;
  case VM_FLOORF: return &&FLOORF - &&opz; break;
  case VM_SINF: return &&SINF - &&opz; break;
  case VM_COSF: return &&COSF - &&opz; break;
  case VM_POWF: return &&POWF - &&opz; break;
  case VM_FABSF: return &&FABSF - &&opz; break;
  case VM_FMODF: return &&FMODF - &&opz; break;
  case VM_LOG10F: return &&LOG10F - &&opz; break;

  case VM_LOAD8:     return &&LOAD8      - &&opz;     break;
  case VM_LOAD8_G:   return &&LOAD8_G    - &&opz;     break;
  case VM_LOAD8_OFF: return &&LOAD8_OFF  - &&opz;     break;
  case VM_LOAD8_ZEXT_32_OFF: return &&LOAD8_ZEXT_32_OFF  - &&opz;     break;
  case VM_LOAD8_SEXT_32_OFF: return &&LOAD8_SEXT_32_OFF  - &&opz;     break;
  case VM_LOAD8_ROFF: return &&LOAD8_ROFF  - &&opz;     break;
  case VM_LOAD8_ZEXT_32_ROFF: return &&LOAD8_ZEXT_32_ROFF  - &&opz;     break;
  case VM_LOAD8_SEXT_32_ROFF: return &&LOAD8_SEXT_32_ROFF  - &&opz;     break;

  case VM_STORE8_G:    return &&STORE8_G    - &&opz;     break;
  case VM_STORE8C_OFF: return &&STORE8C_OFF - &&opz;     break;
  case VM_STORE8_OFF:  return &&STORE8_OFF  - &&opz;     break;
  case VM_STORE8:      return &&STORE8      - &&opz;     break;


  case VM_LOAD16:     return &&LOAD16      - &&opz;     break;
  case VM_LOAD16_G:   return &&LOAD16_G    - &&opz;     break;
  case VM_LOAD16_OFF: return &&LOAD16_OFF  - &&opz;     break;
  case VM_LOAD16_ZEXT_32_OFF: return &&LOAD16_ZEXT_32_OFF  - &&opz;     break;
  case VM_LOAD16_SEXT_32_OFF: return &&LOAD16_SEXT_32_OFF  - &&opz;     break;
  case VM_LOAD16_ROFF: return &&LOAD16_ROFF  - &&opz;     break;
  case VM_LOAD16_ZEXT_32_ROFF: return &&LOAD16_ZEXT_32_ROFF  - &&opz;     break;
  case VM_LOAD16_SEXT_32_ROFF: return &&LOAD16_SEXT_32_ROFF  - &&opz;     break;

  case VM_STORE16_G:    return &&STORE16_G    - &&opz;     break;
  case VM_STORE16C_OFF: return &&STORE16C_OFF - &&opz;     break;
  case VM_STORE16_OFF:  return &&STORE16_OFF  - &&opz;     break;
  case VM_STORE16:      return &&STORE16      - &&opz;     break;


  case VM_LOAD32:    return &&LOAD32     - &&opz;     break;
  case VM_LOAD32_OFF:return &&LOAD32_OFF - &&opz;     break;
  case VM_LOAD32_ROFF:return &&LOAD32_ROFF - &&opz;     break;
  case VM_LOAD32_G:  return &&LOAD32_G   - &&opz;     break;

  case VM_STORE32_G: return &&STORE32_G - &&opz;     break;
  case VM_STORE32C_OFF: return &&STORE32C_OFF - &&opz;     break;
  case VM_STORE32_OFF: return &&STORE32_OFF - &&opz;     break;
  case VM_STORE32: return &&STORE32 - &&opz;     break;

  case VM_LOAD64:    return &&LOAD64     - &&opz;     break;
  case VM_LOAD64_OFF:return &&LOAD64_OFF - &&opz;     break;
  case VM_LOAD64_ROFF:return &&LOAD64_ROFF - &&opz;     break;
  case VM_LOAD64_G:  return &&LOAD64_G   - &&opz;     break;

  case VM_STORE64_G: return &&STORE64_G - &&opz;     break;
  case VM_STORE64C_OFF: return &&STORE64C_OFF - &&opz;     break;
  case VM_STORE64_OFF: return &&STORE64_OFF - &&opz;     break;
  case VM_STORE64: return &&STORE64 - &&opz;     break;

  case VM_EQ8:      return &&EQ8     - &&opz;     break;
  case VM_NE8:      return &&NE8     - &&opz;     break;
  case VM_SGT8:     return &&SGT8    - &&opz;     break;
  case VM_SGE8:     return &&SGE8    - &&opz;     break;
  case VM_SLT8:     return &&SLT8    - &&opz;     break;
  case VM_SLE8:     return &&SLE8    - &&opz;     break;
  case VM_UGT8:     return &&UGT8    - &&opz;     break;
  case VM_UGE8:     return &&UGE8    - &&opz;     break;
  case VM_ULT8:     return &&ULT8    - &&opz;     break;
  case VM_ULE8:     return &&ULE8    - &&opz;     break;

  case VM_EQ8_C:      return &&EQ8_C     - &&opz;     break;
  case VM_NE8_C:      return &&NE8_C     - &&opz;     break;
  case VM_SGT8_C:     return &&SGT8_C    - &&opz;     break;
  case VM_SGE8_C:     return &&SGE8_C    - &&opz;     break;
  case VM_SLT8_C:     return &&SLT8_C    - &&opz;     break;
  case VM_SLE8_C:     return &&SLE8_C    - &&opz;     break;
  case VM_UGT8_C:     return &&UGT8_C    - &&opz;     break;
  case VM_UGE8_C:     return &&UGE8_C    - &&opz;     break;
  case VM_ULT8_C:     return &&ULT8_C    - &&opz;     break;
  case VM_ULE8_C:     return &&ULE8_C    - &&opz;     break;


  case VM_EQ16:      return &&EQ16     - &&opz;     break;
  case VM_NE16:      return &&NE16     - &&opz;     break;
  case VM_SGT16:     return &&SGT16    - &&opz;     break;
  case VM_SGE16:     return &&SGE16    - &&opz;     break;
  case VM_SLT16:     return &&SLT16    - &&opz;     break;
  case VM_SLE16:     return &&SLE16    - &&opz;     break;
  case VM_UGT16:     return &&UGT16    - &&opz;     break;
  case VM_UGE16:     return &&UGE16    - &&opz;     break;
  case VM_ULT16:     return &&ULT16    - &&opz;     break;
  case VM_ULE16:     return &&ULE16    - &&opz;     break;

  case VM_EQ16_C:      return &&EQ16_C     - &&opz;     break;
  case VM_NE16_C:      return &&NE16_C     - &&opz;     break;
  case VM_SGT16_C:     return &&SGT16_C    - &&opz;     break;
  case VM_SGE16_C:     return &&SGE16_C    - &&opz;     break;
  case VM_SLT16_C:     return &&SLT16_C    - &&opz;     break;
  case VM_SLE16_C:     return &&SLE16_C    - &&opz;     break;
  case VM_UGT16_C:     return &&UGT16_C    - &&opz;     break;
  case VM_UGE16_C:     return &&UGE16_C    - &&opz;     break;
  case VM_ULT16_C:     return &&ULT16_C    - &&opz;     break;
  case VM_ULE16_C:     return &&ULE16_C    - &&opz;     break;


  case VM_EQ32:      return &&EQ32     - &&opz;     break;
  case VM_NE32:      return &&NE32     - &&opz;     break;
  case VM_SGT32:     return &&SGT32    - &&opz;     break;
  case VM_SGE32:     return &&SGE32    - &&opz;     break;
  case VM_SLT32:     return &&SLT32    - &&opz;     break;
  case VM_SLE32:     return &&SLE32    - &&opz;     break;
  case VM_UGT32:     return &&UGT32    - &&opz;     break;
  case VM_UGE32:     return &&UGE32    - &&opz;     break;
  case VM_ULT32:     return &&ULT32    - &&opz;     break;
  case VM_ULE32:     return &&ULE32    - &&opz;     break;

  case VM_EQ32_C:      return &&EQ32_C     - &&opz;     break;
  case VM_NE32_C:      return &&NE32_C     - &&opz;     break;
  case VM_SGT32_C:     return &&SGT32_C    - &&opz;     break;
  case VM_SGE32_C:     return &&SGE32_C    - &&opz;     break;
  case VM_SLT32_C:     return &&SLT32_C    - &&opz;     break;
  case VM_SLE32_C:     return &&SLE32_C    - &&opz;     break;
  case VM_UGT32_C:     return &&UGT32_C    - &&opz;     break;
  case VM_UGE32_C:     return &&UGE32_C    - &&opz;     break;
  case VM_ULT32_C:     return &&ULT32_C    - &&opz;     break;
  case VM_ULE32_C:     return &&ULE32_C    - &&opz;     break;

  case VM_EQ64:      return &&EQ64     - &&opz;     break;
  case VM_NE64:      return &&NE64     - &&opz;     break;
  case VM_SGT64:     return &&SGT64    - &&opz;     break;
  case VM_SGE64:     return &&SGE64    - &&opz;     break;
  case VM_SLT64:     return &&SLT64    - &&opz;     break;
  case VM_SLE64:     return &&SLE64    - &&opz;     break;
  case VM_UGT64:     return &&UGT64    - &&opz;     break;
  case VM_UGE64:     return &&UGE64    - &&opz;     break;
  case VM_ULT64:     return &&ULT64    - &&opz;     break;
  case VM_ULE64:     return &&ULE64    - &&opz;     break;

  case VM_EQ64_C:      return &&EQ64_C     - &&opz;     break;
  case VM_NE64_C:      return &&NE64_C     - &&opz;     break;
  case VM_SGT64_C:     return &&SGT64_C    - &&opz;     break;
  case VM_SGE64_C:     return &&SGE64_C    - &&opz;     break;
  case VM_SLT64_C:     return &&SLT64_C    - &&opz;     break;
  case VM_SLE64_C:     return &&SLE64_C    - &&opz;     break;
  case VM_UGT64_C:     return &&UGT64_C    - &&opz;     break;
  case VM_UGE64_C:     return &&UGE64_C    - &&opz;     break;
  case VM_ULT64_C:     return &&ULT64_C    - &&opz;     break;
  case VM_ULE64_C:     return &&ULE64_C    - &&opz;     break;

  case VM_OEQ_DBL:   return &&OEQ_DBL - &&opz;   break;
  case VM_OGT_DBL:   return &&OGT_DBL - &&opz;   break;
  case VM_OGE_DBL:   return &&OGE_DBL - &&opz;   break;
  case VM_OLT_DBL:   return &&OLT_DBL - &&opz;   break;
  case VM_OLE_DBL:   return &&OLE_DBL - &&opz;   break;
  case VM_ONE_DBL:   return &&ONE_DBL - &&opz;   break;
  case VM_ORD_DBL:   return &&ORD_DBL - &&opz;   break;
  case VM_UNO_DBL:   return &&UNO_DBL - &&opz;   break;
  case VM_UEQ_DBL:   return &&UEQ_DBL - &&opz;   break;
  case VM_UGT_DBL:   return &&UGT_DBL - &&opz;   break;
  case VM_UGE_DBL:   return &&UGE_DBL - &&opz;   break;
  case VM_ULT_DBL:   return &&ULT_DBL - &&opz;   break;
  case VM_ULE_DBL:   return &&ULE_DBL - &&opz;   break;
  case VM_UNE_DBL:   return &&UNE_DBL - &&opz;   break;

  case VM_OEQ_DBL_C:   return &&OEQ_DBL_C - &&opz;   break;
  case VM_OGT_DBL_C:   return &&OGT_DBL_C - &&opz;   break;
  case VM_OGE_DBL_C:   return &&OGE_DBL_C - &&opz;   break;
  case VM_OLT_DBL_C:   return &&OLT_DBL_C - &&opz;   break;
  case VM_OLE_DBL_C:   return &&OLE_DBL_C - &&opz;   break;
  case VM_ONE_DBL_C:   return &&ONE_DBL_C - &&opz;   break;
  case VM_ORD_DBL_C:   return &&ORD_DBL_C - &&opz;   break;
  case VM_UNO_DBL_C:   return &&UNO_DBL_C - &&opz;   break;
  case VM_UEQ_DBL_C:   return &&UEQ_DBL_C - &&opz;   break;
  case VM_UGT_DBL_C:   return &&UGT_DBL_C - &&opz;   break;
  case VM_UGE_DBL_C:   return &&UGE_DBL_C - &&opz;   break;
  case VM_ULT_DBL_C:   return &&ULT_DBL_C - &&opz;   break;
  case VM_ULE_DBL_C:   return &&ULE_DBL_C - &&opz;   break;
  case VM_UNE_DBL_C:   return &&UNE_DBL_C - &&opz;   break;

  case VM_OEQ_FLT:   return &&OEQ_FLT - &&opz;   break;
  case VM_OGT_FLT:   return &&OGT_FLT - &&opz;   break;
  case VM_OGE_FLT:   return &&OGE_FLT - &&opz;   break;
  case VM_OLT_FLT:   return &&OLT_FLT - &&opz;   break;
  case VM_OLE_FLT:   return &&OLE_FLT - &&opz;   break;
  case VM_ONE_FLT:   return &&ONE_FLT - &&opz;   break;
  case VM_ORD_FLT:   return &&ORD_FLT - &&opz;   break;
  case VM_UNO_FLT:   return &&UNO_FLT - &&opz;   break;
  case VM_UEQ_FLT:   return &&UEQ_FLT - &&opz;   break;
  case VM_UGT_FLT:   return &&UGT_FLT - &&opz;   break;
  case VM_UGE_FLT:   return &&UGE_FLT - &&opz;   break;
  case VM_ULT_FLT:   return &&ULT_FLT - &&opz;   break;
  case VM_ULE_FLT:   return &&ULE_FLT - &&opz;   break;
  case VM_UNE_FLT:   return &&UNE_FLT - &&opz;   break;

  case VM_OEQ_FLT_C:   return &&OEQ_FLT_C - &&opz;   break;
  case VM_OGT_FLT_C:   return &&OGT_FLT_C - &&opz;   break;
  case VM_OGE_FLT_C:   return &&OGE_FLT_C - &&opz;   break;
  case VM_OLT_FLT_C:   return &&OLT_FLT_C - &&opz;   break;
  case VM_OLE_FLT_C:   return &&OLE_FLT_C - &&opz;   break;
  case VM_ONE_FLT_C:   return &&ONE_FLT_C - &&opz;   break;
  case VM_ORD_FLT_C:   return &&ORD_FLT_C - &&opz;   break;
  case VM_UNO_FLT_C:   return &&UNO_FLT_C - &&opz;   break;
  case VM_UEQ_FLT_C:   return &&UEQ_FLT_C - &&opz;   break;
  case VM_UGT_FLT_C:   return &&UGT_FLT_C - &&opz;   break;
  case VM_UGE_FLT_C:   return &&UGE_FLT_C - &&opz;   break;
  case VM_ULT_FLT_C:   return &&ULT_FLT_C - &&opz;   break;
  case VM_ULE_FLT_C:   return &&ULE_FLT_C - &&opz;   break;
  case VM_UNE_FLT_C:   return &&UNE_FLT_C - &&opz;   break;

  case VM_EQ8_BR:      return &&EQ8_BR     - &&opz;     break;
  case VM_NE8_BR:      return &&NE8_BR     - &&opz;     break;
  case VM_SGT8_BR:     return &&SGT8_BR    - &&opz;     break;
  case VM_SGE8_BR:     return &&SGE8_BR    - &&opz;     break;
  case VM_SLT8_BR:     return &&SLT8_BR    - &&opz;     break;
  case VM_SLE8_BR:     return &&SLE8_BR    - &&opz;     break;
  case VM_UGT8_BR:     return &&UGT8_BR    - &&opz;     break;
  case VM_UGE8_BR:     return &&UGE8_BR    - &&opz;     break;
  case VM_ULT8_BR:     return &&ULT8_BR    - &&opz;     break;
  case VM_ULE8_BR:     return &&ULE8_BR    - &&opz;     break;

  case VM_EQ8_C_BR:      return &&EQ8_C_BR     - &&opz;     break;
  case VM_NE8_C_BR:      return &&NE8_C_BR     - &&opz;     break;
  case VM_SGT8_C_BR:     return &&SGT8_C_BR    - &&opz;     break;
  case VM_SGE8_C_BR:     return &&SGE8_C_BR    - &&opz;     break;
  case VM_SLT8_C_BR:     return &&SLT8_C_BR    - &&opz;     break;
  case VM_SLE8_C_BR:     return &&SLE8_C_BR    - &&opz;     break;
  case VM_UGT8_C_BR:     return &&UGT8_C_BR    - &&opz;     break;
  case VM_UGE8_C_BR:     return &&UGE8_C_BR    - &&opz;     break;
  case VM_ULT8_C_BR:     return &&ULT8_C_BR    - &&opz;     break;
  case VM_ULE8_C_BR:     return &&ULE8_C_BR    - &&opz;     break;

  case VM_EQ32_BR:      return &&EQ32_BR     - &&opz;     break;
  case VM_NE32_BR:      return &&NE32_BR     - &&opz;     break;
  case VM_SGT32_BR:     return &&SGT32_BR    - &&opz;     break;
  case VM_SGE32_BR:     return &&SGE32_BR    - &&opz;     break;
  case VM_SLT32_BR:     return &&SLT32_BR    - &&opz;     break;
  case VM_SLE32_BR:     return &&SLE32_BR    - &&opz;     break;
  case VM_UGT32_BR:     return &&UGT32_BR    - &&opz;     break;
  case VM_UGE32_BR:     return &&UGE32_BR    - &&opz;     break;
  case VM_ULT32_BR:     return &&ULT32_BR    - &&opz;     break;
  case VM_ULE32_BR:     return &&ULE32_BR    - &&opz;     break;

  case VM_EQ32_C_BR:      return &&EQ32_C_BR     - &&opz;     break;
  case VM_NE32_C_BR:      return &&NE32_C_BR     - &&opz;     break;
  case VM_SGT32_C_BR:     return &&SGT32_C_BR    - &&opz;     break;
  case VM_SGE32_C_BR:     return &&SGE32_C_BR    - &&opz;     break;
  case VM_SLT32_C_BR:     return &&SLT32_C_BR    - &&opz;     break;
  case VM_SLE32_C_BR:     return &&SLE32_C_BR    - &&opz;     break;
  case VM_UGT32_C_BR:     return &&UGT32_C_BR    - &&opz;     break;
  case VM_UGE32_C_BR:     return &&UGE32_C_BR    - &&opz;     break;
  case VM_ULT32_C_BR:     return &&ULT32_C_BR    - &&opz;     break;
  case VM_ULE32_C_BR:     return &&ULE32_C_BR    - &&opz;     break;


  case VM_SELECT8RR: return &&SELECT8RR - &&opz;     break;
  case VM_SELECT8RC: return &&SELECT8RC - &&opz;     break;
  case VM_SELECT8CR: return &&SELECT8CR - &&opz;     break;
  case VM_SELECT8CC: return &&SELECT8CC - &&opz;     break;

  case VM_SELECT16RR: return &&SELECT16RR - &&opz;     break;
  case VM_SELECT16RC: return &&SELECT16RC - &&opz;     break;
  case VM_SELECT16CR: return &&SELECT16CR - &&opz;     break;
  case VM_SELECT16CC: return &&SELECT16CC - &&opz;     break;

  case VM_SELECT32RR: return &&SELECT32RR - &&opz;     break;
  case VM_SELECT32RC: return &&SELECT32RC - &&opz;     break;
  case VM_SELECT32CR: return &&SELECT32CR - &&opz;     break;
  case VM_SELECT32CC: return &&SELECT32CC - &&opz;     break;

  case VM_SELECT64RR: return &&SELECT64RR - &&opz;     break;
  case VM_SELECT64RC: return &&SELECT64RC - &&opz;     break;
  case VM_SELECT64CR: return &&SELECT64CR - &&opz;     break;
  case VM_SELECT64CC: return &&SELECT64CC - &&opz;     break;

  case VM_B:         return &&B        - &&opz;     break;
  case VM_BCOND:     return &&BCOND    - &&opz;     break;
  case VM_JSR_VM:    return &&JSR_VM   - &&opz;     break;
  case VM_JSR_EXT:   return &&JSR_EXT  - &&opz;     break;
  case VM_JSR_R:     return &&JSR_R    - &&opz;     break;

  case VM_MOV8:      return &&MOV8     - &&opz;     break;
  case VM_MOV32:     return &&MOV32    - &&opz;     break;
  case VM_MOV64:     return &&MOV64    - &&opz;     break;
  case VM_MOV8_C:    return &&MOV8_C   - &&opz;     break;
  case VM_MOV16_C:   return &&MOV16_C  - &&opz;     break;
  case VM_MOV32_C:   return &&MOV32_C  - &&opz;     break;
  case VM_MOV64_C:   return &&MOV64_C  - &&opz;     break;

  case VM_LEA_R32_SHL:     return &&LEA_R32_SHL - &&opz;break;
  case VM_LEA_R32_SHL2:    return &&LEA_R32_SHL2 - &&opz;break;
  case VM_LEA_R32_SHL_OFF: return &&LEA_R32_SHL_OFF - &&opz;break;
  case VM_LEA_R32_MUL_OFF: return &&LEA_R32_MUL_OFF - &&opz;break;

  case VM_CAST_1_TRUNC_8:  return &&CAST_1_TRUNC_8 - &&opz;  break;
  case VM_CAST_1_TRUNC_16: return &&CAST_1_TRUNC_16 - &&opz;  break;

  case VM_CAST_8_ZEXT_1:   return &&CAST_8_ZEXT_1 - &&opz; break;
  case VM_CAST_8_TRUNC_16: return &&CAST_8_TRUNC_16 - &&opz;  break;
  case VM_CAST_8_TRUNC_32: return &&CAST_8_TRUNC_32 - &&opz;  break;
  case VM_CAST_8_TRUNC_64: return &&CAST_8_TRUNC_64 - &&opz;  break;

  case VM_CAST_16_ZEXT_1:   return &&CAST_16_ZEXT_1 - &&opz; break;
  case VM_CAST_16_ZEXT_8:   return &&CAST_16_ZEXT_8 - &&opz; break;
  case VM_CAST_16_SEXT_8:   return &&CAST_16_SEXT_8 - &&opz; break;
  case VM_CAST_16_FPTOSI_FLT: return &&CAST_16_FPTOSI_FLT - &&opz; break;
  case VM_CAST_16_FPTOUI_FLT: return &&CAST_16_FPTOUI_FLT - &&opz; break;
  case VM_CAST_16_FPTOSI_DBL: return &&CAST_16_FPTOSI_DBL - &&opz; break;
  case VM_CAST_16_FPTOUI_DBL: return &&CAST_16_FPTOUI_DBL - &&opz; break;
  case VM_CAST_16_TRUNC_32: return &&CAST_16_TRUNC_32 - &&opz; break;
  case VM_CAST_16_TRUNC_64: return &&CAST_16_TRUNC_64 - &&opz; break;

  case VM_CAST_32_SEXT_1: return &&CAST_32_SEXT_1 - &&opz;  break;
  case VM_CAST_32_ZEXT_8: return &&CAST_32_ZEXT_8 - &&opz;  break;
  case VM_CAST_32_SEXT_8: return &&CAST_32_SEXT_8 - &&opz;  break;
  case VM_CAST_32_ZEXT_16: return &&CAST_32_ZEXT_16 - &&opz;  break;
  case VM_CAST_32_SEXT_16: return &&CAST_32_SEXT_16 - &&opz;  break;
  case VM_CAST_32_TRUNC_64: return &&CAST_32_TRUNC_64 - &&opz;  break;
  case VM_CAST_32_FPTOSI_FLT: return &&CAST_32_FPTOSI_FLT - &&opz; break;
  case VM_CAST_32_FPTOUI_FLT: return &&CAST_32_FPTOUI_FLT - &&opz; break;
  case VM_CAST_32_FPTOSI_DBL: return &&CAST_32_FPTOSI_DBL - &&opz; break;
  case VM_CAST_32_FPTOUI_DBL: return &&CAST_32_FPTOUI_DBL - &&opz; break;

  case VM_CAST_64_ZEXT_1: return &&CAST_64_ZEXT_1 - &&opz;  break;
  case VM_CAST_64_SEXT_1: return &&CAST_64_SEXT_1 - &&opz;  break;
  case VM_CAST_64_ZEXT_8: return &&CAST_64_ZEXT_8 - &&opz;  break;
  case VM_CAST_64_SEXT_8: return &&CAST_64_SEXT_8 - &&opz;  break;
  case VM_CAST_64_ZEXT_16: return &&CAST_64_ZEXT_16 - &&opz;  break;
  case VM_CAST_64_SEXT_16: return &&CAST_64_SEXT_16 - &&opz;  break;
  case VM_CAST_64_ZEXT_32: return &&CAST_64_ZEXT_32 - &&opz;  break;
  case VM_CAST_64_SEXT_32: return &&CAST_64_SEXT_32 - &&opz;  break;
  case VM_CAST_64_FPTOSI_FLT: return &&CAST_64_FPTOSI_FLT - &&opz; break;
  case VM_CAST_64_FPTOUI_FLT: return &&CAST_64_FPTOUI_FLT - &&opz; break;
  case VM_CAST_64_FPTOSI_DBL: return &&CAST_64_FPTOSI_DBL - &&opz; break;
  case VM_CAST_64_FPTOUI_DBL: return &&CAST_64_FPTOUI_DBL - &&opz; break;


  case VM_CAST_FLT_FPTRUNC_DBL: return &&CAST_FLT_FPTRUNC_DBL - &&opz; break;
  case VM_CAST_FLT_SITOFP_8:    return &&CAST_FLT_SITOFP_8  - &&opz; break;
  case VM_CAST_FLT_UITOFP_8:    return &&CAST_FLT_UITOFP_8  - &&opz; break;
  case VM_CAST_FLT_SITOFP_16:   return &&CAST_FLT_SITOFP_16 - &&opz; break;
  case VM_CAST_FLT_UITOFP_16:   return &&CAST_FLT_UITOFP_16 - &&opz; break;
  case VM_CAST_FLT_SITOFP_32:   return &&CAST_FLT_SITOFP_32 - &&opz; break;
  case VM_CAST_FLT_UITOFP_32:   return &&CAST_FLT_UITOFP_32 - &&opz; break;
  case VM_CAST_FLT_SITOFP_64:   return &&CAST_FLT_SITOFP_64 - &&opz; break;
  case VM_CAST_FLT_UITOFP_64:   return &&CAST_FLT_UITOFP_64 - &&opz; break;

  case VM_CAST_DBL_SITOFP_8:    return &&CAST_DBL_SITOFP_8  - &&opz; break;
  case VM_CAST_DBL_UITOFP_8:    return &&CAST_DBL_UITOFP_8  - &&opz; break;
  case VM_CAST_DBL_SITOFP_16:   return &&CAST_DBL_SITOFP_16 - &&opz; break;
  case VM_CAST_DBL_UITOFP_16:   return &&CAST_DBL_UITOFP_16 - &&opz; break;
  case VM_CAST_DBL_SITOFP_32:   return &&CAST_DBL_SITOFP_32 - &&opz; break;
  case VM_CAST_DBL_UITOFP_32:   return &&CAST_DBL_UITOFP_32 - &&opz; break;
  case VM_CAST_DBL_SITOFP_64:   return &&CAST_DBL_SITOFP_64 - &&opz; break;
  case VM_CAST_DBL_UITOFP_64:   return &&CAST_DBL_UITOFP_64 - &&opz; break;

  case VM_CAST_DBL_FPEXT_FLT:   return &&CAST_DBL_FPEXT_FLT   - &&opz; break;


  case VM_JUMPTABLE:   return &&JUMPTABLE   - &&opz; break;
  case VM_SWITCH8_BS:  return &&SWITCH8_BS  - &&opz; break;
  case VM_SWITCH32_BS: return &&SWITCH32_BS - &&opz; break;
  case VM_SWITCH64_BS: return &&SWITCH64_BS - &&opz; break;
  case VM_ALLOCA:   return &&ALLOCA - &&opz; break;
  case VM_ALLOCAD:  return &&ALLOCAD - &&opz; break;
  case VM_VASTART:  return &&VASTART - &&opz; break;
  case VM_VAARG32:  return &&VAARG32 - &&opz; break;
  case VM_VAARG64:  return &&VAARG64 - &&opz; break;
  case VM_VACOPY:   return &&VACOPY  - &&opz; break;

  case VM_STACKSHRINK: return &&STACKSHRINK - &&opz; break;
  case VM_STACKSAVE:  return &&STACKSAVE - &&opz; break;
  case VM_STACKRESTORE:  return &&STACKRESTORE - &&opz; break;
  case VM_STACKCOPYR: return &&STACKCOPYR - &&opz; break;
  case VM_STACKCOPYC: return &&STACKCOPYC - &&opz; break;

  case VM_MEMCPY:   return &&MEMCPY  - &&opz; break;
  case VM_MEMSET:   return &&MEMSET  - &&opz; break;

  case VM_LLVM_MEMCPY:   return &&LLVM_MEMCPY  - &&opz; break;
  case VM_LLVM_MEMSET:   return &&LLVM_MEMSET  - &&opz; break;
  case VM_LLVM_MEMSET64: return &&LLVM_MEMSET64 - &&opz; break;

  case VM_CTZ32: return &&CTZ32 - &&opz; break;
  case VM_CLZ32: return &&CLZ32 - &&opz; break;
  case VM_POP32: return &&POP32 - &&opz; break;

  case VM_CTZ64: return &&CTZ64 - &&opz; break;
  case VM_CLZ64: return &&CLZ64 - &&opz; break;
  case VM_POP64: return &&POP64 - &&opz; break;

  case VM_UADDO32: return &&UADDO32 - &&opz; break;

  case VM_MEMMOVE:  return &&MEMMOVE - &&opz; break;
  case VM_MEMCMP:   return &&MEMCMP  - &&opz; break;

  case VM_STRCMP:   return &&STRCMP  - &&opz; break;
  case VM_STRNCMP:  return &&STRNCMP - &&opz; break;
  case VM_STRCPY:   return &&STRCPY  - &&opz; break;
  case VM_STRNCPY:  return &&STRNCPY - &&opz; break;
  case VM_STRCHR:   return &&STRCHR  - &&opz; break;
  case VM_STRRCHR:  return &&STRRCHR - &&opz; break;
  case VM_STRLEN:   return &&STRLEN  - &&opz; break;

  case VM_UNREACHABLE: return &&UNREACHABLE - &&opz; break;

  case VM_INSTRUMENT_COUNT: return &&INSTRUMENT_COUNT - &&opz; break;

  default:
    printf("Can't emit op %d\n", op);
    abort();
  }
#endif
}


static int16_t
vm_resolve(int op)
{
  int o = vm_exec(NULL, NULL, NULL, NULL, 0, op);
  assert(o <= INT16_MAX);
  assert(o >= INT16_MIN);
  return o;
}

/**
 *
 */
static void
emit_i16(ir_unit_t *iu, uint16_t i16)
{
  if(iu->iu_text_ptr + 2 >= iu->iu_text_alloc + iu->iu_text_alloc_memsize)
    parser_error(iu, "Function too big");
  *(uint16_t *)iu->iu_text_ptr = i16;
  iu->iu_text_ptr += 2;
}

/**
 *
 */
static void
emit_i8(ir_unit_t *iu, uint8_t i8)
{
  if(iu->iu_text_ptr + 2 >= iu->iu_text_alloc + iu->iu_text_alloc_memsize)
    parser_error(iu, "Function too big");
  *(uint8_t *)iu->iu_text_ptr = i8;
  iu->iu_text_ptr += 2; // We always align to 16 bits
}

/**
 *
 */
static void
emit_i32(ir_unit_t *iu, uint32_t i32)
{
  if(iu->iu_text_ptr + 4 >= iu->iu_text_alloc + iu->iu_text_alloc_memsize)
    parser_error(iu, "Function too big");
  *(uint32_t *)iu->iu_text_ptr = i32;
  iu->iu_text_ptr += 4;
}

/**
 *
 */
static void
emit_i64(ir_unit_t *iu, uint64_t i64)
{
  if(iu->iu_text_ptr + 8 >= iu->iu_text_alloc + iu->iu_text_alloc_memsize)
    parser_error(iu, "Function too big");
  *(uint64_t *)iu->iu_text_ptr = i64;
  iu->iu_text_ptr += 8;
}

/**
 *
 */
static void *
emit_data(ir_unit_t *iu, int size)
{
  if(iu->iu_text_ptr + size >= iu->iu_text_alloc + iu->iu_text_alloc_memsize)
    parser_error(iu, "Function too big");
  void *r = iu->iu_text_ptr;
  iu->iu_text_ptr += size;
  return r;
}


/**
 *
 */
static void
emit_op(ir_unit_t *iu, vm_op_t op)
{
  emit_i16(iu, vm_resolve(op));
}


/**
 *
 */
static void
emit_op1(ir_unit_t *iu, vm_op_t op, uint16_t arg)
{
  emit_i16(iu, vm_resolve(op));
  emit_i16(iu, arg);
}


/**
 *
 */
static void __attribute__((unused))
emit_op2(ir_unit_t *iu, vm_op_t op,
         uint16_t a1, uint16_t a2)
{
  emit_i16(iu, vm_resolve(op));
  emit_i16(iu, a1);
  emit_i16(iu, a2);
}

/**
 *
 */
static void
emit_op3(ir_unit_t *iu, vm_op_t op,
         uint16_t a1, uint16_t a2, uint16_t a3)
{
  emit_i16(iu, vm_resolve(op));
  emit_i16(iu, a1);
  emit_i16(iu, a2);
  emit_i16(iu, a3);
}

/**
 *
 */
static void
emit_op4(ir_unit_t *iu, vm_op_t op,
         uint16_t a1, uint16_t a2, uint16_t a3, uint16_t a4)
{
  emit_i16(iu, vm_resolve(op));
  emit_i16(iu, a1);
  emit_i16(iu, a2);
  emit_i16(iu, a3);
  emit_i16(iu, a4);
}


/**
 *
 */
static void
vm_align32(ir_unit_t *iu, int imm_at_odd)
{
  int text_is_odd = !!((intptr_t)iu->iu_text_ptr & 2);
  if(imm_at_odd != text_is_odd)
    emit_op(iu, VM_NOP);
}


/**
 *
 */
static void
emit_ret(ir_unit_t *iu, ir_instr_unary_t *ii)
{
  if(ii->value == -1) {
    emit_op(iu, VM_RET_VOID);
    return;
  }
  ir_value_t *iv = value_get(iu, ii->value);
  ir_type_t *it = type_get(iu, iv->iv_type);

  switch(iv->iv_class) {
  case IR_VC_REGFRAME:

    switch(it->it_code) {
    case IR_TYPE_INT8:
      emit_op1(iu, VM_RET_R8, value_reg(iv));
      return;
    case IR_TYPE_INT16:
      emit_op1(iu, VM_RET_R16, value_reg(iv));
      return;
    case IR_TYPE_INT1:
    case IR_TYPE_INT32:
    case IR_TYPE_POINTER:
    case IR_TYPE_FLOAT:
      emit_op1(iu, VM_RET_R32, value_reg(iv));
      return;

    case IR_TYPE_INT64:
    case IR_TYPE_DOUBLE:
      emit_op1(iu, VM_RET_R64, value_reg(iv));
      return;

    default:
      parser_error(iu, "Can't return type %s", type_str(iu, it));
    }

  case IR_VC_GLOBALVAR:
    switch(it->it_code) {
    case IR_TYPE_POINTER:
      emit_op(iu, VM_RET_R32C);
      emit_i32(iu, value_get_const32(iu, iv));
      return;

    default:
      parser_error(iu, "Can't return global type %s", type_str(iu, it));
    }

  case IR_VC_CONSTANT:
    switch(it->it_code) {
    case IR_TYPE_INT1:
    case IR_TYPE_INT8:
    case IR_TYPE_INT16:
    case IR_TYPE_INT32:
    case IR_TYPE_POINTER:
    case IR_TYPE_FLOAT:
      emit_op(iu, VM_RET_R32C);
      emit_i32(iu, value_get_const32(iu, iv));
      return;
    case IR_TYPE_INT64:
    case IR_TYPE_DOUBLE:
      vm_align32(iu, 1);
      emit_op(iu, VM_RET_R64C);
      emit_i64(iu, value_get_const64(iu, iv));
      return;

    default:
      parser_error(iu, "Can't return const type %s", type_str(iu, it));
    }

  case IR_VC_FUNCTION:
    emit_op(iu, VM_RET_R32C);
    emit_i32(iu, value_function_addr(iv));
    return;

  default:
    parser_error(iu, "Can't return value class %d", iv->iv_class);
  }
}



/**
 *
 */
static void
emit_binop(ir_unit_t *iu, ir_instr_binary_t *ii)
{
  const int binop = ii->op;
  const ir_value_t *lhs = value_get(iu, ii->lhs_value);
  const ir_value_t *rhs = value_get(iu, ii->rhs_value);
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  vm_op_t op;
  const ir_type_t *it = type_get(iu, lhs->iv_type);

  if(lhs->iv_class == IR_VC_REGFRAME &&
     rhs->iv_class == IR_VC_REGFRAME) {

    int lhsreg = value_reg(lhs);
    int retreg = value_reg(ret);

    switch(it->it_code) {

    case IR_TYPE_INT1:
    case IR_TYPE_INT32:
      if(lhsreg == ACCPOS && retreg == ACCPOS) {
        op = VM_ADD_2ACC_R32 + binop;
        emit_op1(iu, op, value_reg(rhs));
        iu->iu_stats.vm_binop_acc_acc++;
      } else if(lhsreg == ACCPOS) {
        op = VM_ADD_ACC_R32 + binop;
        emit_op2(iu, op, value_reg(ret), value_reg(rhs));
        iu->iu_stats.vm_binop_acc++;
      } else {
        op = VM_ADD_R32 + binop;
        emit_op3(iu, op, value_reg(ret), lhsreg, value_reg(rhs));
      }
      return;

    case IR_TYPE_INT8:
      op = VM_ADD_R8 + binop;
      emit_op3(iu, op, value_reg(ret), value_reg(lhs), value_reg(rhs));
      return;

    case IR_TYPE_INT16:
      op = VM_ADD_R16 + binop;
      emit_op3(iu, op, value_reg(ret), value_reg(lhs), value_reg(rhs));
      return;

    case IR_TYPE_INT64:
      op = VM_ADD_R64 + binop;
      emit_op3(iu, op, value_reg(ret), value_reg(lhs), value_reg(rhs));
      return;

    case IR_TYPE_DOUBLE:

      switch(binop) {
      case BINOP_ADD:  op = VM_ADD_DBL; break;
      case BINOP_SUB:  op = VM_SUB_DBL; break;
      case BINOP_MUL:  op = VM_MUL_DBL; break;
      case BINOP_SDIV:
      case BINOP_UDIV: op = VM_DIV_DBL; break;
      default:
        parser_error(iu, "Can't binop %d for double", binop);
      }
      emit_op3(iu, op, value_reg(ret), value_reg(lhs), value_reg(rhs));
      break;

    case IR_TYPE_FLOAT:

      switch(binop) {
      case BINOP_ADD:  op = VM_ADD_FLT; break;
      case BINOP_SUB:  op = VM_SUB_FLT; break;
      case BINOP_MUL:  op = VM_MUL_FLT; break;
      case BINOP_SDIV:
      case BINOP_UDIV: op = VM_DIV_FLT; break;
      default:
        parser_error(iu, "Can't binop %d for float", binop);
      }
      emit_op3(iu, op, value_reg(ret), value_reg(lhs), value_reg(rhs));
      break;


    default:
      parser_error(iu, "Can't binop types %s", type_str(iu, it));
    }

  } else if(lhs->iv_class == IR_VC_REGFRAME &&
            rhs->iv_class == IR_VC_CONSTANT) {

    int lhsreg = value_reg(lhs);
    int retreg = value_reg(ret);

    switch(it->it_code) {
    case IR_TYPE_INT8:
      op = VM_ADD_R8C + binop;
      emit_op2(iu, op, value_reg(ret), value_reg(lhs));
      emit_i8(iu, value_get_const32(iu, rhs));
      return;

    case IR_TYPE_INT16:
      op = VM_ADD_R16C + binop;
      emit_op2(iu, op, value_reg(ret), value_reg(lhs));
      emit_i16(iu, value_get_const32(iu, rhs));
      return;

    case IR_TYPE_INT1:
    case IR_TYPE_INT32:
      {
        const uint32_t u32 = value_get_const32(iu, rhs);
        if((binop == BINOP_ADD && u32 == 1) ||
           (binop == BINOP_SUB && u32 == -1)) {
          emit_op2(iu, VM_INC_R32, value_reg(ret), value_reg(lhs));
          return;
        }
        if((binop == BINOP_ADD && u32 == -1) ||
           (binop == BINOP_SUB && u32 == 1)) {
          emit_op2(iu, VM_DEC_R32, value_reg(ret), value_reg(lhs));
          return;
        }

        if(lhsreg == ACCPOS && retreg == ACCPOS) {
          op = VM_ADD_2ACC_R32C + binop;
          emit_op(iu, op);
          iu->iu_stats.vm_binop_acc_imm++;
        } else if(lhsreg == ACCPOS) {
          op = VM_ADD_ACC_R32C + binop;
          emit_op1(iu, op, value_reg(ret));
          iu->iu_stats.vm_binop_acc_acc_imm++;
        } else {
          op = VM_ADD_R32C + binop;
          emit_op2(iu, op, value_reg(ret), lhsreg);
        }
        emit_i32(iu, value_get_const32(iu, rhs));
      }
      return;

    case IR_TYPE_INT64:
      vm_align32(iu, 1);
      op = VM_ADD_R64C + binop;
      emit_op2(iu, op, value_reg(ret), value_reg(lhs));
      emit_i64(iu, value_get_const64(iu, rhs));
      return;

    case IR_TYPE_DOUBLE:

      switch(binop) {
      case BINOP_ADD:  op = VM_ADD_DBLC; break;
      case BINOP_SUB:  op = VM_SUB_DBLC; break;
      case BINOP_MUL:  op = VM_MUL_DBLC; break;
      case BINOP_SDIV:
      case BINOP_UDIV: op = VM_DIV_DBLC; break;
      default:
        parser_error(iu, "Can't binop %d for double", binop);
      }
      vm_align32(iu, 1);
      emit_op2(iu, op, value_reg(ret), value_reg(lhs));
      emit_i64(iu, value_get_const64(iu, rhs));
      break;

    case IR_TYPE_FLOAT:

      switch(binop) {
      case BINOP_ADD:  op = VM_ADD_FLTC; break;
      case BINOP_SUB:  op = VM_SUB_FLTC; break;
      case BINOP_MUL:  op = VM_MUL_FLTC; break;
      case BINOP_SDIV:
      case BINOP_UDIV: op = VM_DIV_FLTC; break;
      default:
        parser_error(iu, "Can't binop %d for float", binop);
      }
      vm_align32(iu, 1);
      emit_op2(iu, op, value_reg(ret), value_reg(lhs));
      emit_i32(iu, value_get_const32(iu, rhs));
      break;

    default:
      parser_error(iu, "Can't binop types %s", type_str(iu, it));
    }

  } else {
    parser_error(iu, "Can't binop value class %d and %d",
                 lhs->iv_class, rhs->iv_class);
  }
}

/**
 *
 */
static void
emit_load(ir_unit_t *iu, ir_instr_load_t *ii)
{
  const ir_value_t *src = value_get(iu, ii->ptr);
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_value_t *roff =
    ii->value_offset >= 0 ? value_get(iu, ii->value_offset) : NULL;

  if(ii->cast != -1) {
    // Load + Cast
    ir_type_t *pointee = type_get(iu, ii->load_type);
    ir_type_t *retty = type_get(iu, ret->iv_type);
    assert(src->iv_class == IR_VC_REGFRAME);

    switch(COMBINE3(retty->it_code, pointee->it_code, ii->cast)) {
    case COMBINE3(IR_TYPE_INT32, IR_TYPE_INT8, CAST_ZEXT):
      emit_op2(iu, roff ? VM_LOAD8_ZEXT_32_ROFF :
               VM_LOAD8_ZEXT_32_OFF, value_reg(ret), value_reg(src));
      emit_i16(iu, ii->immediate_offset);
      break;
    case COMBINE3(IR_TYPE_INT32, IR_TYPE_INT8, CAST_SEXT):
      emit_op2(iu, roff ? VM_LOAD8_SEXT_32_ROFF : VM_LOAD8_SEXT_32_OFF,
               value_reg(ret), value_reg(src));
      emit_i16(iu, ii->immediate_offset);
      break;
    case COMBINE3(IR_TYPE_INT32, IR_TYPE_INT16, CAST_ZEXT):
      emit_op2(iu, roff ? VM_LOAD16_ZEXT_32_ROFF: VM_LOAD16_ZEXT_32_OFF,
               value_reg(ret), value_reg(src));
      emit_i16(iu, ii->immediate_offset);
      break;
    case COMBINE3(IR_TYPE_INT32, IR_TYPE_INT16, CAST_SEXT):
      emit_op2(iu, roff ? VM_LOAD16_SEXT_32_ROFF : VM_LOAD16_SEXT_32_OFF,
               value_reg(ret), value_reg(src));
      emit_i16(iu, ii->immediate_offset);
      break;
    default:
      parser_error(iu, "Can't load+cast to %s from %s cast:%d",
                   type_str(iu, retty),
                   type_str(iu, pointee),
                   ii->cast);
    }
    if(roff != NULL) {
      emit_i16(iu, value_reg(roff));
      emit_i16(iu, ii->value_offset_multiply);
    }
    return;
  }

  ir_type_t *pointee = type_get(iu, ret->iv_type);
  const int has_offset = ii->immediate_offset != 0 || roff != NULL;

  switch(COMBINE3(src->iv_class, pointee->it_code, has_offset)) {

  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT8, 0):
    emit_op2(iu, VM_LOAD8, value_reg(ret), value_reg(src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT8, 1):
    emit_op2(iu, roff ? VM_LOAD8_ROFF : VM_LOAD8_OFF,
             value_reg(ret), value_reg(src));
    emit_i16(iu, ii->immediate_offset);
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_INT8, 0):
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_INT8, 0):
    emit_op1(iu, VM_LOAD8_G, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;

  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_INT1, 0):
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_INT1, 0):
    emit_op1(iu, VM_LOAD8_G, 0);
    emit_i32(iu, value_get_const32(iu, src));
    emit_op2(iu, VM_CAST_1_TRUNC_8, value_reg(ret), 0);
    return;

  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT16, 0):
    emit_op2(iu, VM_LOAD16, value_reg(ret), value_reg(src));
    return;
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_INT16, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_INT16, 0):
    emit_op1(iu, VM_LOAD16_G, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT16, 1):
    emit_op2(iu, roff ? VM_LOAD16_ROFF : VM_LOAD16_OFF,
             value_reg(ret), value_reg(src));
    emit_i16(iu, ii->immediate_offset);
    break;

  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_INT32, 0):
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_POINTER, 0):
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_FLOAT, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_INT32, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_POINTER, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_FLOAT, 0):
    emit_op1(iu, VM_LOAD32_G, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT32, 0):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_POINTER, 0):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_FLOAT, 0):
    emit_op2(iu, VM_LOAD32, value_reg(ret), value_reg(src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT32, 1):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_POINTER, 1):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_FLOAT, 1):
    emit_op2(iu, roff ? VM_LOAD32_ROFF : VM_LOAD32_OFF,
             value_reg(ret), value_reg(src));
    emit_i16(iu, ii->immediate_offset);
    break;


  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_INT64, 0):
  case COMBINE3(IR_VC_GLOBALVAR, IR_TYPE_DOUBLE, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_INT64, 0):
  case COMBINE3(IR_VC_CONSTANT, IR_TYPE_DOUBLE, 0):
    emit_op1(iu, VM_LOAD64_G, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT64, 0):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_DOUBLE, 0):
    emit_op2(iu, VM_LOAD64, value_reg(ret), value_reg(src));
    return;
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_INT64, 1):
  case COMBINE3(IR_VC_REGFRAME, IR_TYPE_DOUBLE, 1):
    emit_op2(iu, roff ? VM_LOAD64_ROFF : VM_LOAD64_OFF,
             value_reg(ret), value_reg(src));
    emit_i16(iu, ii->immediate_offset);
    break;

  default:
    instr_print(iu, &ii->super, 0);
    printf("\n");
    parser_error(iu, "Can't load from class %d %s immediate-offset:%d",
                 src->iv_class, type_str(iu, pointee),
                 has_offset);
  }
  if(roff != NULL) {
    emit_i16(iu, value_reg(roff));
    emit_i16(iu, ii->value_offset_multiply);
  }
}


/**
 *
 */
static void
emit_store(ir_unit_t *iu, ir_instr_store_t *ii)
{
  const ir_value_t *ptr = value_get(iu, ii->ptr);
  const ir_value_t *val = value_get(iu, ii->value);
  int has_offset = ii->offset != 0;

  switch(COMBINE4(type_get(iu, val->iv_type)->it_code,
                  val->iv_class,
                  ptr->iv_class,
                  has_offset)) {

    // ---

  case COMBINE4(IR_TYPE_INT8, IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
    emit_op2(iu, VM_STORE8, value_reg(ptr), value_reg(val));
    return;

  case COMBINE4(IR_TYPE_INT8, IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
    emit_op2(iu, VM_STORE8_OFF, value_reg(ptr), value_reg(val));
    emit_i16(iu, ii->offset);
    return;

  case COMBINE4(IR_TYPE_INT8, IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_INT8, IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
    emit_op1(iu, VM_STORE8_G, value_reg(val));
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT1, IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_INT8, IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_INT8, IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
    emit_op1(iu, VM_MOV8_C, 0);
    emit_i8(iu, value_get_const32(iu, val));
    emit_op1(iu, VM_STORE8_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT8, IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_INT8, IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
    emit_op1(iu, VM_STORE8C_OFF, value_reg(ptr));
    emit_i16(iu, ii->offset);
    emit_i8(iu, value_get_const32(iu, val));
    return;


    // ---

  case COMBINE4(IR_TYPE_INT16, IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
    emit_op2(iu, VM_STORE16, value_reg(ptr), value_reg(val));
    return;

  case COMBINE4(IR_TYPE_INT16, IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_INT16, IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
    emit_op1(iu, VM_STORE16_G, value_reg(val));
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT16, IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
    emit_op2(iu, VM_STORE16_OFF, value_reg(ptr), value_reg(val));
    emit_i16(iu, ii->offset);
    return;

  case COMBINE4(IR_TYPE_INT16, IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_INT16, IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
    emit_op1(iu, VM_STORE16C_OFF, value_reg(ptr));
    emit_i16(iu, ii->offset);
    emit_i16(iu, value_get_const32(iu, val));
    return;

  case COMBINE4(IR_TYPE_INT16, IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_INT16, IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
    emit_op1(iu, VM_MOV16_C, 0);
    emit_i16(iu, value_get_const32(iu, val));
    emit_op1(iu, VM_STORE16_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

    // ---

  case COMBINE4(IR_TYPE_INT32,   IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_INT32,   IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
    emit_op1(iu, VM_STORE32_G, value_reg(val));
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT32,   IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_GLOBALVAR, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_GLOBALVAR, IR_VC_GLOBALVAR, 0):
    emit_op1(iu, VM_MOV32_C, 0);
    emit_i32(iu, value_get_const32(iu, val));
    emit_op1(iu, VM_STORE32_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT32,   IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
    emit_op1(iu, VM_MOV32_C, 0);
    emit_i32(iu, value_get_const32(iu, val));
    emit_op1(iu, VM_STORE32_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT32,   IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_INT32,   IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_INT32,   IR_VC_GLOBALVAR, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_INT32,   IR_VC_GLOBALVAR, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_GLOBALVAR, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_GLOBALVAR, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_GLOBALVAR, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_GLOBALVAR, IR_VC_REGFRAME, 0):
    emit_op1(iu, VM_STORE32C_OFF, value_reg(ptr));
    emit_i16(iu, ii->offset);
    emit_i32(iu, value_get_const32(iu, val));
    return;


  case COMBINE4(IR_TYPE_INT32,   IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
    emit_op2(iu, VM_STORE32_OFF, value_reg(ptr), value_reg(val));
    emit_i16(iu, ii->offset);
    return;

  case COMBINE4(IR_TYPE_INT32,   IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_POINTER, IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_FLOAT,   IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
    emit_op2(iu, VM_STORE32, value_reg(ptr), value_reg(val));
    return;

    // ---

  case COMBINE4(IR_TYPE_INT64,  IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_REGFRAME, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_INT64,  IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_REGFRAME, IR_VC_CONSTANT, 0):
    emit_op1(iu, VM_STORE64_G, value_reg(val));
    emit_i32(iu, value_get_const32(iu, ptr));
    return;

  case COMBINE4(IR_TYPE_INT64,  IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_CONSTANT, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_INT64,  IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_CONSTANT, IR_VC_GLOBALVAR, 0):
    vm_align32(iu, 0);
    emit_op1(iu, VM_MOV64_C, 0);
    emit_i64(iu, value_get_const64(iu, val));
    emit_op1(iu, VM_STORE64_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;


  case COMBINE4(IR_TYPE_INT64,  IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_INT64,  IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_CONSTANT, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_CONSTANT, IR_VC_REGFRAME, 0):
    vm_align32(iu, 1);
    emit_op1(iu, VM_STORE64C_OFF, value_reg(ptr));
    emit_i16(iu, ii->offset);
    emit_i64(iu, value_get_const64(iu, val));
    return;

  case COMBINE4(IR_TYPE_INT64,  IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_REGFRAME, IR_VC_REGFRAME, 1):
    emit_op2(iu, VM_STORE64_OFF, value_reg(ptr), value_reg(val));
    emit_i16(iu, ii->offset);
    return;

  case COMBINE4(IR_TYPE_INT64,  IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_DOUBLE, IR_VC_REGFRAME, IR_VC_REGFRAME, 0):
    emit_op2(iu, VM_STORE64, value_reg(ptr), value_reg(val));
    return;


    // ----

  case COMBINE4(IR_TYPE_FUNCTION, IR_VC_FUNCTION, IR_VC_REGFRAME, 0):
  case COMBINE4(IR_TYPE_FUNCTION, IR_VC_FUNCTION, IR_VC_REGFRAME, 1):
    emit_op1(iu, VM_STORE32C_OFF, value_reg(ptr));
    emit_i16(iu, ii->offset);
    emit_i32(iu, value_function_addr(val));
    return;

  case COMBINE4(IR_TYPE_FUNCTION, IR_VC_FUNCTION, IR_VC_CONSTANT, 0):
  case COMBINE4(IR_TYPE_FUNCTION, IR_VC_FUNCTION, IR_VC_GLOBALVAR, 0):
    emit_op1(iu, VM_MOV32_C, 0);
    emit_i32(iu, value_function_addr(val));
    emit_op1(iu, VM_STORE32_G, 0);
    emit_i32(iu, value_get_const32(iu, ptr));
    return;


  default:
    instr_print(iu, &ii->super, 0);
    printf("\n");
    parser_error(iu, "Can't store (type %s class %d) ptr class %d off:%d",
                 type_str_index(iu, val->iv_type),
                 val->iv_class,
                 ptr->iv_class, has_offset);
  }
}


static enum Predicate
swapPred(enum Predicate pred)
{
  switch (pred) {
  default:
    abort();
    case ICMP_EQ: case ICMP_NE:
      return pred;
    case ICMP_SGT: return ICMP_SLT;
    case ICMP_SLT: return ICMP_SGT;
    case ICMP_SGE: return ICMP_SLE;
    case ICMP_SLE: return ICMP_SGE;
    case ICMP_UGT: return ICMP_ULT;
    case ICMP_ULT: return ICMP_UGT;
    case ICMP_UGE: return ICMP_ULE;
    case ICMP_ULE: return ICMP_UGE;
    case FCMP_FALSE: case FCMP_TRUE:
    case FCMP_OEQ: case FCMP_ONE:
    case FCMP_UEQ: case FCMP_UNE:
    case FCMP_ORD: case FCMP_UNO:
      return pred;
    case FCMP_OGT: return FCMP_OLT;
    case FCMP_OLT: return FCMP_OGT;
    case FCMP_OGE: return FCMP_OLE;
    case FCMP_OLE: return FCMP_OGE;
    case FCMP_UGT: return FCMP_ULT;
    case FCMP_ULT: return FCMP_UGT;
    case FCMP_UGE: return FCMP_ULE;
    case FCMP_ULE: return FCMP_UGE;
  }
}



/**
 *
 */
static void
emit_cmp2(ir_unit_t *iu, ir_instr_binary_t *ii)
{
  enum Predicate pred = ii->op;
  const ir_value_t *lhs = value_get(iu, ii->lhs_value);
  const ir_value_t *rhs = value_get(iu, ii->rhs_value);
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_type_t *it = type_get(iu, lhs->iv_type);

  if(lhs->iv_class == IR_VC_REGFRAME &&
     rhs->iv_class == IR_VC_REGFRAME) {

    if(pred >= FCMP_OEQ && pred <= FCMP_UNE) {
      switch(it->it_code) {
      case IR_TYPE_FLOAT:
        emit_op3(iu, pred - FCMP_OEQ + VM_OEQ_FLT,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      case IR_TYPE_DOUBLE:
        emit_op3(iu, pred - FCMP_OEQ + VM_OEQ_DBL,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      default:
        parser_error(iu, "Can't fcmp type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
    } else if(pred >= ICMP_EQ && pred <= ICMP_SLE) {

      switch(it->it_code) {
      case IR_TYPE_INT8:
        emit_op3(iu, pred - ICMP_EQ + VM_EQ8,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      case IR_TYPE_INT16:
        emit_op3(iu, pred - ICMP_EQ + VM_EQ16,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      case IR_TYPE_INT32:
      case IR_TYPE_POINTER:
        emit_op3(iu, pred - ICMP_EQ + VM_EQ32,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      case IR_TYPE_INT64:
        emit_op3(iu, pred - ICMP_EQ + VM_EQ64,
                 value_reg(ret), value_reg(lhs), value_reg(rhs));
        return;

      default:
        parser_error(iu, "Can't icmp type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
    } else {
      parser_error(iu, "Can't compare pred %d", pred);
    }

  } else if((lhs->iv_class == IR_VC_REGFRAME &&
             rhs->iv_class == IR_VC_CONSTANT) ||
            (lhs->iv_class == IR_VC_CONSTANT &&
             rhs->iv_class == IR_VC_REGFRAME)) {


    if(rhs->iv_class == IR_VC_REGFRAME) {
      // Swap LHS RHS
      const ir_value_t *tmp = rhs;
      rhs = lhs;
      lhs = tmp;
      pred = swapPred(pred);
    }

    if(pred >= FCMP_OEQ && pred <= FCMP_UNE) {
      switch(it->it_code) {

      case IR_TYPE_DOUBLE:
        if(__builtin_isnan(rhs->iv_double))
          parser_error(iu, "Ugh, immediate is nan in fcmp");
        vm_align32(iu, 1);
        emit_op2(iu, pred - FCMP_OEQ + VM_OEQ_DBL_C,
                 value_reg(ret), value_reg(lhs));
        emit_i64(iu, value_get_const64(iu, rhs));
        return;

      case IR_TYPE_FLOAT:
        if(__builtin_isnan(rhs->iv_float))
          parser_error(iu, "Ugh, immediate is nan in fcmp");

        vm_align32(iu, 1);
        emit_op2(iu, pred - FCMP_OEQ + VM_OEQ_FLT_C,
                 value_reg(ret), value_reg(lhs));
        emit_i32(iu, value_get_const32(iu, rhs));
        return;

      default:
        parser_error(iu, "Can't fcmp type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }

    } else if(pred >= ICMP_EQ && pred <= ICMP_SLE) {

      switch(it->it_code) {
      case IR_TYPE_INT8:
        emit_op2(iu, pred - ICMP_EQ + VM_EQ8_C,
                 value_reg(ret), value_reg(lhs));
        emit_i8(iu, value_get_const32(iu, rhs));
        return;

      case IR_TYPE_INT16:
        emit_op2(iu, pred - ICMP_EQ + VM_EQ16_C,
                 value_reg(ret), value_reg(lhs));
        emit_i16(iu, value_get_const32(iu, rhs));
        return;

      case IR_TYPE_INT32:
      case IR_TYPE_POINTER:
        emit_op2(iu, pred - ICMP_EQ + VM_EQ32_C,
                 value_reg(ret), value_reg(lhs));
        emit_i32(iu, value_get_const32(iu, rhs));
        return;

      case IR_TYPE_INT64:
        vm_align32(iu, 1);
        emit_op2(iu, pred - ICMP_EQ + VM_EQ64_C,
                 value_reg(ret), value_reg(lhs));
        emit_i64(iu, value_get_const64(iu, rhs));
        return;

      default:
        parser_error(iu, "Can't icmp type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
    } else {
      parser_error(iu, "Can't compare pred %d (const)", pred);
    }


  } else if((lhs->iv_class == IR_VC_REGFRAME &&
             rhs->iv_class == IR_VC_GLOBALVAR) ||
            (lhs->iv_class == IR_VC_GLOBALVAR &&
             rhs->iv_class == IR_VC_REGFRAME)) {


    if(lhs->iv_class == IR_VC_GLOBALVAR) {
      // Swap LHS RHS
      const ir_value_t *tmp = rhs;
      rhs = lhs;
      lhs = tmp;
      pred = swapPred(pred);
    }

    if(pred >= ICMP_EQ && pred <= ICMP_SLE) {

      switch(it->it_code) {
      case IR_TYPE_POINTER:
        emit_op2(iu, pred - ICMP_EQ + VM_EQ32_C,
                 value_reg(ret), value_reg(lhs));
        emit_i32(iu, value_get_const32(iu, rhs));
        return;

      default:
        parser_error(iu, "Can't icmp type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
    } else {
      parser_error(iu, "Can't compare pred %d (const)", pred);
    }


  } else {
    parser_error(iu, "Can't icmp value class %d and %d",
                 lhs->iv_class, rhs->iv_class);
  }
}



/**
 *
 */
static void
emit_cmp_branch(ir_unit_t *iu, ir_instr_cmp_branch_t *ii)
{
  enum Predicate pred = ii->op;
  const ir_value_t *lhs = value_get(iu, ii->lhs_value);
  const ir_value_t *rhs = value_get(iu, ii->rhs_value);

  const ir_type_t *it = type_get(iu, lhs->iv_type);

  int textpos = iu->iu_text_ptr - iu->iu_text_alloc;
  VECTOR_PUSH_BACK(&iu->iu_branch_fixups, textpos);

  if(lhs->iv_class == IR_VC_REGFRAME &&
     rhs->iv_class == IR_VC_REGFRAME) {

    if(pred >= ICMP_EQ && pred <= ICMP_SLE) {

      switch(it->it_code) {
      case IR_TYPE_INT8:
        emit_i16(iu, pred - ICMP_EQ + VM_EQ8_BR);
        break;

      case IR_TYPE_INT32:
      case IR_TYPE_POINTER:
        emit_i16(iu, pred - ICMP_EQ + VM_EQ32_BR);
        break;

      default:
        parser_error(iu, "Can't cmpbr type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
      emit_i16(iu, ii->true_branch);
      emit_i16(iu, ii->false_branch);
      emit_i16(iu, value_reg(lhs));
      emit_i16(iu, value_reg(rhs));


    } else {
      parser_error(iu, "Can't compare pred %d", pred);
    }

  } else if(lhs->iv_class == IR_VC_REGFRAME &&
            (rhs->iv_class == IR_VC_CONSTANT ||
             rhs->iv_class == IR_VC_GLOBALVAR)) {

    if(pred >= ICMP_EQ && pred <= ICMP_SLE) {

      switch(it->it_code) {
      case IR_TYPE_INT8:
        emit_i16(iu, pred - ICMP_EQ + VM_EQ8_C_BR);
        emit_i16(iu, ii->true_branch);
        emit_i16(iu, ii->false_branch);
        emit_i16(iu, value_reg(lhs));
        emit_i8(iu, value_get_const32(iu, rhs));
        break;

      case IR_TYPE_INT32:
      case IR_TYPE_POINTER:
        emit_i16(iu, pred - ICMP_EQ + VM_EQ32_C_BR);
        emit_i16(iu, ii->true_branch);
        emit_i16(iu, ii->false_branch);
        emit_i16(iu, value_reg(lhs));
        emit_i32(iu, value_get_const32(iu, rhs));
        break;

      default:
        parser_error(iu, "Can't cmpbr type %s class %d/%d op %d",
                     type_str_index(iu, lhs->iv_type),
                     lhs->iv_class, rhs->iv_class, pred);
      }
    } else {
      parser_error(iu, "Can't brcmp pred %d (const)", pred);
    }

  } else if(lhs->iv_class == IR_VC_REGFRAME &&
            rhs->iv_class == IR_VC_FUNCTION) {

    emit_i16(iu, pred - ICMP_EQ + VM_EQ32_C_BR);
    emit_i16(iu, ii->true_branch);
    emit_i16(iu, ii->false_branch);
    emit_i16(iu, value_reg(lhs));
    emit_i32(iu, value_function_addr(rhs));

  } else {
    parser_error(iu, "Can't brcmp value class %d and %d",
                 lhs->iv_class, rhs->iv_class);
  }
}


/**
 *
 */
static void
emit_br(ir_unit_t *iu, ir_instr_br_t *ii)
{
  int textpos = iu->iu_text_ptr - iu->iu_text_alloc;
  VECTOR_PUSH_BACK(&iu->iu_branch_fixups, textpos);

  // We can't emit code yet cause we don't know the final destination
  if(ii->condition == -1) {
    // Unconditional branch
    emit_i16(iu, VM_B);
    emit_i16(iu, ii->true_branch);
  } else {
    // Conditional branch

    ir_value_t *iv = value_get(iu, ii->condition);
    switch(iv->iv_class) {
    case IR_VC_REGFRAME:
      emit_i16(iu, VM_BCOND);
      emit_i16(iu, value_reg(iv));
      emit_i16(iu, ii->true_branch);
      emit_i16(iu, ii->false_branch);
      break;

    case IR_VC_CONSTANT:
      emit_i16(iu, VM_B);
      if(value_get_const32(iu, iv))
        emit_i16(iu, ii->true_branch);
      else
        emit_i16(iu, ii->false_branch);
      break;
    default:
      parser_error(iu, "Unable to branch on value class %d", iv->iv_class);
    }
  }
}


/**
 *
 */
static void
emit_switch(ir_unit_t *iu, ir_instr_switch_t *ii)
{
  const ir_value_t *c = value_get(iu, ii->value);
  const ir_type_t *cty = type_get(iu, c->iv_type);

  uint32_t mask32 = 0xffffffff;
  int jumptable_size = 0;

  assert(c->iv_class == IR_VC_REGFRAME);
  int reg = value_reg(c);

  switch(cty->it_code) {

  case IR_TYPE_INT8:

    assert(type_get(iu, c->iv_type)->it_code == IR_TYPE_INT8);

    if(cty->it_bits <= 4) {
      jumptable_size = 1 << ii->width;
      goto jumptable;
    }

    if(ii->num_paths >= ii->num_paths / 4) {
      jumptable_size = 256;
      goto jumptable;
    }

    VECTOR_PUSH_BACK(&iu->iu_branch_fixups,
                     iu->iu_text_ptr - iu->iu_text_alloc);
    emit_i16(iu, VM_SWITCH8_BS);
    emit_i16(iu, reg);
    emit_i16(iu, ii->num_paths);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i8(iu, ii->paths[n].v64);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i16(iu, ii->paths[n].block);

    emit_i16(iu, ii->defblock);
    break;

  case IR_TYPE_INT16:
    assert(c->iv_class == IR_VC_REGFRAME);
    assert(type_get(iu, c->iv_type)->it_code == IR_TYPE_INT16);
    emit_op2(iu, VM_CAST_16_TRUNC_32, 0, reg);
    reg = 0;
    mask32 = 0xffff;
    goto switch32;


  case IR_TYPE_INT32:

  switch32:
    VECTOR_PUSH_BACK(&iu->iu_branch_fixups,
                     iu->iu_text_ptr - iu->iu_text_alloc);

    emit_i16(iu, VM_SWITCH32_BS);

    emit_i16(iu, reg);
    emit_i16(iu, ii->num_paths);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i32(iu, ii->paths[n].v64 & mask32);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i16(iu, ii->paths[n].block);

    emit_i16(iu, ii->defblock);
    break;

  case IR_TYPE_INT64:
    assert(c->iv_class == IR_VC_REGFRAME);
    assert(type_get(iu, c->iv_type)->it_code == IR_TYPE_INT64);

    vm_align32(iu, 1);

    VECTOR_PUSH_BACK(&iu->iu_branch_fixups,
                     iu->iu_text_ptr - iu->iu_text_alloc);
    emit_i16(iu, VM_SWITCH64_BS);
    emit_i16(iu, reg);
    emit_i16(iu, ii->num_paths);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i64(iu, ii->paths[n].v64);

    for(int n = 0; n < ii->num_paths; n++)
      emit_i16(iu, ii->paths[n].block);

    emit_i16(iu, ii->defblock);
    break;


  jumptable:
    VECTOR_PUSH_BACK(&iu->iu_branch_fixups,
                     iu->iu_text_ptr - iu->iu_text_alloc);
    emit_i16(iu, VM_JUMPTABLE);
    emit_i16(iu, reg);
    emit_i16(iu, jumptable_size);
    const int mask = jumptable_size - 1;
    int16_t *table = emit_data(iu, jumptable_size * 2);

    // Fill table with default paths
    for(int i = 0; i < jumptable_size; i++)
      table[i] = ii->defblock;

    for(int n = 0; n < ii->num_paths; n++)
      table[ii->paths[n].v64 & mask] = ii->paths[n].block;

    break;

  default:
    parser_error(iu, "Bad type %s, in switch (%d paths)",
                 ii->typecode, ii->num_paths);
  }
}


/**
 *
 */
static void
emit_move(ir_unit_t *iu, ir_instr_move_t *ii)
{
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_value_t *src = value_get(iu, ii->value);
  ir_type_t *ty = type_get(iu, src->iv_type);

  switch(COMBINE2(src->iv_class, ty->it_code)) {
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_INT8):
    emit_op1(iu, VM_MOV8_C, value_reg(ret));
    emit_i8(iu, value_get_const32(iu, src));
    return;
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_INT16):
    emit_op1(iu, VM_MOV16_C, value_reg(ret));
    emit_i16(iu, value_get_const32(iu, src));
    return;

  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_INT1):
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_INT32):
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_POINTER):
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_FLOAT):
    emit_op1(iu, VM_MOV32_C, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;

  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_INT64):
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_DOUBLE):
    vm_align32(iu, 0);
    emit_op1(iu, VM_MOV64_C, value_reg(ret));
    emit_i64(iu, value_get_const64(iu, src));
    return;

  case COMBINE2(IR_VC_GLOBALVAR, IR_TYPE_POINTER):
    emit_op1(iu, VM_MOV32_C, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    return;

  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_INT1):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_INT8):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_INT16):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_INT32):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_POINTER):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_FLOAT):
    emit_op2(iu, VM_MOV32, value_reg(ret), value_reg(src));
    return;
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_INT64):
  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_DOUBLE):
    emit_op2(iu, VM_MOV64, value_reg(ret), value_reg(src));
    return;

  case COMBINE2(IR_VC_FUNCTION, IR_TYPE_FUNCTION):
    emit_op1(iu, VM_MOV32_C, value_reg(ret));
    emit_i32(iu, value_function_addr(src));
    break;
  default:
    parser_error(iu, "Can't move from %s class %d",
                 type_str(iu, ty), src->iv_class);
  }
}


/**
 *
 */
static void
emit_stackcopy(ir_unit_t *iu, ir_instr_stackcopy_t *ii)
{
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_value_t *src = value_get(iu, ii->value);
  ir_type_t *ty = type_get(iu, src->iv_type);

  switch(COMBINE2(src->iv_class, ty->it_code)) {

  case COMBINE2(IR_VC_GLOBALVAR, IR_TYPE_POINTER):
  case COMBINE2(IR_VC_CONSTANT, IR_TYPE_POINTER):
    emit_op1(iu, VM_STACKCOPYC, value_reg(ret));
    emit_i32(iu, value_get_const32(iu, src));
    emit_i32(iu, ii->size);
    return;

  case COMBINE2(IR_VC_REGFRAME, IR_TYPE_POINTER):
    emit_op2(iu, VM_STACKCOPYR, value_reg(ret), value_reg(src));
    emit_i32(iu, ii->size);
    return;
  default:
    parser_error(iu, "Can't stackcopy from %s class %d",
                 type_str(iu, ty), src->iv_class);
  }
}


/**
 *
 */
static void
emit_stackshrink(ir_unit_t *iu, ir_instr_stackshrink_t *ii)
{
  emit_op(iu, VM_STACKSHRINK);
  emit_i32(iu, ii->size);
}


/**
 *
 */
static void
emit_lea(ir_unit_t *iu, ir_instr_lea_t *ii)
{
  const ir_value_t *    ret = value_get(iu, ii->super.ii_ret_value);
  const ir_value_t *baseptr = value_get(iu, ii->baseptr);

  if(ii->value_offset == -1) {

    // Lea with immediate offset is same as add32 with constant
    emit_op2(iu, VM_ADD_R32C, value_reg(ret), value_reg(baseptr));
    emit_i32(iu, ii->immediate_offset);

  } else {
    const ir_value_t *off = value_get(iu, ii->value_offset);

    if(type_get(iu, off->iv_type)->it_code != IR_TYPE_INT32) {
      parser_error(iu, "LEA: Can't handle %s as offset register",
                   type_str_index(iu, off->iv_type));
    }

    assert(ii->value_offset_multiply != 0);

    int fb = ffs(ii->value_offset_multiply) - 1;
    if((1 << fb) == ii->value_offset_multiply) {

      if(ii->immediate_offset) {
        emit_op4(iu, VM_LEA_R32_SHL_OFF,
                 value_reg(ret), value_reg(baseptr), value_reg(off), fb);
        emit_i32(iu, ii->immediate_offset);
        return;
      }

      if(fb == 2) {
        emit_op3(iu, VM_LEA_R32_SHL2,
               value_reg(ret), value_reg(baseptr), value_reg(off));
        return;
      }
      emit_op4(iu, VM_LEA_R32_SHL,
               value_reg(ret), value_reg(baseptr), value_reg(off), fb);
      return;
    }

    emit_op3(iu, VM_LEA_R32_MUL_OFF,
             value_reg(ret), value_reg(baseptr), value_reg(off));
    emit_i32(iu, ii->value_offset_multiply);
    emit_i32(iu, ii->immediate_offset);
  }
}


/**
 *
 */
static void
emit_cast(ir_unit_t *iu, ir_instr_unary_t *ii)
{
  const ir_value_t *src = value_get(iu, ii->value);
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_type_t *it;
  const int srccode = type_get(iu, src->iv_type)->it_code;
  const int dstcode = type_get(iu, ret->iv_type)->it_code;
  const int castop = ii->op;

  vm_op_t op;
  switch(COMBINE3(dstcode, castop, srccode)) {

  case COMBINE3(IR_TYPE_INT1, CAST_TRUNC, IR_TYPE_INT8):
    op = VM_CAST_1_TRUNC_8;
    break;

  case COMBINE3(IR_TYPE_INT1, CAST_TRUNC, IR_TYPE_INT16):
    op = VM_CAST_1_TRUNC_16;
    break;

    
  case COMBINE3(IR_TYPE_INT8, CAST_TRUNC, IR_TYPE_INT16):
    op = VM_CAST_8_TRUNC_16;
    break;
  case COMBINE3(IR_TYPE_INT8, CAST_TRUNC, IR_TYPE_INT32):
  case COMBINE3(IR_TYPE_INT8, CAST_ZEXT,  IR_TYPE_INT1):
    op = VM_CAST_8_TRUNC_32;
    break;
  case COMBINE3(IR_TYPE_INT8, CAST_TRUNC, IR_TYPE_INT64):
    op = VM_CAST_8_TRUNC_64;
    break;

  case COMBINE3(IR_TYPE_INT16, CAST_ZEXT, IR_TYPE_INT1):
    op = VM_CAST_16_ZEXT_1;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_ZEXT, IR_TYPE_INT8):
    op = VM_CAST_16_ZEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_SEXT, IR_TYPE_INT8):
    op = VM_CAST_16_SEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_TRUNC, IR_TYPE_INT32):
    op = VM_CAST_16_TRUNC_32;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_TRUNC, IR_TYPE_INT64):
    op = VM_CAST_16_TRUNC_64;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_FPTOSI, IR_TYPE_FLOAT):
    op = VM_CAST_16_FPTOSI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_FPTOUI, IR_TYPE_FLOAT):
    op = VM_CAST_16_FPTOUI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_FPTOSI, IR_TYPE_DOUBLE):
    op = VM_CAST_16_FPTOSI_DBL;
    break;
  case COMBINE3(IR_TYPE_INT16, CAST_FPTOUI, IR_TYPE_DOUBLE):
    op = VM_CAST_16_FPTOUI_DBL;
    break;


  case COMBINE3(IR_TYPE_INT32, CAST_TRUNC, IR_TYPE_INT64):
    op = VM_CAST_32_TRUNC_64;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_SEXT, IR_TYPE_INT1):
    op = VM_CAST_32_SEXT_1;
    break;

  case COMBINE3(IR_TYPE_INT32, CAST_ZEXT, IR_TYPE_INT8):
    op = VM_CAST_32_ZEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_SEXT, IR_TYPE_INT8):
    op = VM_CAST_32_SEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_ZEXT, IR_TYPE_INT16):
    op = VM_CAST_32_ZEXT_16;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_SEXT, IR_TYPE_INT16):
    op = VM_CAST_32_SEXT_16;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_FPTOSI, IR_TYPE_FLOAT):
    op = VM_CAST_32_FPTOSI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_FPTOUI, IR_TYPE_FLOAT):
    op = VM_CAST_32_FPTOUI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_FPTOSI, IR_TYPE_DOUBLE):
    op = VM_CAST_32_FPTOSI_DBL;
    break;
  case COMBINE3(IR_TYPE_INT32, CAST_FPTOUI, IR_TYPE_DOUBLE):
    op = VM_CAST_32_FPTOUI_DBL;
    break;



  case COMBINE3(IR_TYPE_INT64, CAST_ZEXT, IR_TYPE_INT1):
    op = VM_CAST_64_ZEXT_1;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_SEXT, IR_TYPE_INT1):
    op = VM_CAST_64_SEXT_1;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_ZEXT, IR_TYPE_INT8):
    op = VM_CAST_64_ZEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_SEXT, IR_TYPE_INT8):
    op = VM_CAST_64_SEXT_8;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_ZEXT, IR_TYPE_INT16):
    op = VM_CAST_64_ZEXT_16;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_SEXT, IR_TYPE_INT16):
    op = VM_CAST_64_SEXT_16;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_ZEXT, IR_TYPE_INT32):
    op = VM_CAST_64_ZEXT_32;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_SEXT, IR_TYPE_INT32):
    op = VM_CAST_64_SEXT_32;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_FPTOSI, IR_TYPE_FLOAT):
    op = VM_CAST_64_FPTOSI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_FPTOUI, IR_TYPE_FLOAT):
    op = VM_CAST_64_FPTOUI_FLT;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_FPTOSI, IR_TYPE_DOUBLE):
    op = VM_CAST_64_FPTOSI_DBL;
    break;
  case COMBINE3(IR_TYPE_INT64, CAST_FPTOUI, IR_TYPE_DOUBLE):
    op = VM_CAST_64_FPTOUI_DBL;
    break;


  case COMBINE3(IR_TYPE_FLOAT, CAST_FPTRUNC, IR_TYPE_DOUBLE):
    op = VM_CAST_FLT_FPTRUNC_DBL;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_SITOFP, IR_TYPE_INT8):
    op = VM_CAST_FLT_SITOFP_8;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_UITOFP, IR_TYPE_INT8):
    op = VM_CAST_FLT_UITOFP_8;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_SITOFP, IR_TYPE_INT16):
    op = VM_CAST_FLT_SITOFP_16;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_UITOFP, IR_TYPE_INT16):
    op = VM_CAST_FLT_UITOFP_16;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_SITOFP, IR_TYPE_INT32):
    op = VM_CAST_FLT_SITOFP_32;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_UITOFP, IR_TYPE_INT32):
    op = VM_CAST_FLT_UITOFP_32;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_SITOFP, IR_TYPE_INT64):
    op = VM_CAST_FLT_SITOFP_64;
    break;
  case COMBINE3(IR_TYPE_FLOAT, CAST_UITOFP, IR_TYPE_INT64):
    op = VM_CAST_FLT_UITOFP_64;
    break;

  case COMBINE3(IR_TYPE_DOUBLE, CAST_SITOFP, IR_TYPE_INT8):
    op = VM_CAST_DBL_SITOFP_8;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_UITOFP, IR_TYPE_INT8):
    op = VM_CAST_DBL_UITOFP_8;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_SITOFP, IR_TYPE_INT16):
    op = VM_CAST_DBL_SITOFP_16;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_UITOFP, IR_TYPE_INT16):
    op = VM_CAST_DBL_UITOFP_16;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_SITOFP, IR_TYPE_INT32):
    op = VM_CAST_DBL_SITOFP_32;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_UITOFP, IR_TYPE_INT32):
    op = VM_CAST_DBL_UITOFP_32;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_SITOFP, IR_TYPE_INT64):
    op = VM_CAST_DBL_SITOFP_64;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_UITOFP, IR_TYPE_INT64):
    op = VM_CAST_DBL_UITOFP_64;
    break;
  case COMBINE3(IR_TYPE_DOUBLE, CAST_FPEXT, IR_TYPE_FLOAT):
    op = VM_CAST_DBL_FPEXT_FLT;
    break;


    // optimization candidates
  case COMBINE3(IR_TYPE_INT32, CAST_ZEXT, IR_TYPE_INT1):
  case COMBINE3(IR_TYPE_POINTER, CAST_BITCAST, IR_TYPE_POINTER):
  case COMBINE3(IR_TYPE_POINTER, CAST_INTTOPTR, IR_TYPE_INT32):
  case COMBINE3(IR_TYPE_INT32, CAST_PTRTOINT, IR_TYPE_POINTER):
    emit_op2(iu, VM_MOV32, value_reg(ret), value_reg(src));
    return;

  case COMBINE3(IR_TYPE_DOUBLE, CAST_BITCAST, IR_TYPE_INT64):
  case COMBINE3(IR_TYPE_INT64,  CAST_BITCAST, IR_TYPE_DOUBLE):
    emit_op2(iu, VM_MOV64, value_reg(ret), value_reg(src));
    return;

  case COMBINE3(IR_TYPE_INT64, CAST_PTRTOINT, IR_TYPE_POINTER):
    op = VM_CAST_64_ZEXT_32;
    return;

  case COMBINE3(IR_TYPE_INTx, CAST_TRUNC, IR_TYPE_INT32):
    it = type_get(iu, ret->iv_type);
    if(it->it_bits <= 8) {
      op = VM_CAST_8_TRUNC_32;
      break;
    }
    emit_op2(iu, VM_MOV32, value_reg(ret), value_reg(src));
    return;

  case COMBINE3(IR_TYPE_INTx, CAST_TRUNC, IR_TYPE_INT8):
    emit_op2(iu, VM_MOV8, value_reg(ret), value_reg(src));
    return;

  default:
    parser_error(iu, "Unable to convert to %s from %s using castop %d",
                 type_str_index(iu, ret->iv_type),
                 type_str_index(iu, src->iv_type),
                 castop);
  }
  emit_op2(iu, op, value_reg(ret), value_reg(src));
}


/**
 *
 */
static void
emit_call(ir_unit_t *iu, ir_instr_call_t *ii, ir_function_t *f)
{
  int rf_offset = f->if_regframe_size;
  int return_reg;

  if(ii->super.ii_ret_value != -1) {
    const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
    return_reg = value_reg(ret);
  } else {
    return_reg = 0;
  }
  ir_function_t *callee = value_function(iu, ii->callee);
  if(callee != NULL) {
    vm_op_t op;

    if(callee->if_ext_func != NULL)
      op = VM_JSR_EXT;
    else
      op = VM_JSR_VM;

    emit_op3(iu, op, callee->if_gfid, rf_offset, return_reg);

  } else {

    const ir_value_t *iv = value_get(iu, ii->callee);

    if(iv->iv_class != IR_VC_REGFRAME)
      parser_error(iu, "Call via incompatible value class %d",
                   iv->iv_class);
    emit_op3(iu, VM_JSR_R, value_reg(iv), rf_offset, return_reg);
  }
}


/**
 *
 */
static void
emit_alloca(ir_unit_t *iu, ir_instr_alloca_t *ii)
{
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  ir_value_t *iv = value_get(iu, ii->num_items_value);

  switch(iv->iv_class) {
  case IR_VC_CONSTANT:
    emit_op2(iu, VM_ALLOCA, value_reg(ret), ii->alignment);
    emit_i32(iu, ii->size * value_get_const32(iu, iv));
    break;

  case IR_VC_REGFRAME:
    switch(type_get(iu, iv->iv_type)->it_code) {
    case IR_TYPE_INT32:
      emit_op3(iu, VM_ALLOCAD, value_reg(ret), ii->alignment, value_reg(iv));
      emit_i32(iu, ii->size);
      break;

    default:
      parser_error(iu, "Unable to alloca num_elements as %s",
                   type_str_index(iu, iv->iv_type));
    }
    return;

  default:
    parser_error(iu, "Bad class %d for alloca elements",
                 iv->iv_class);
  }

}


/**
 *
 */
static void
emit_select(ir_unit_t *iu, ir_instr_select_t *ii)
{
  const ir_value_t *p  = value_get(iu, ii->pred);
  const ir_value_t *tv = value_get(iu, ii->true_value);
  const ir_value_t *fv = value_get(iu, ii->false_value);
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  //  assert(tv->iv_type == fv->iv_type);

  switch(COMBINE3(tv->iv_class, fv->iv_class,
                  type_get(iu, tv->iv_type)->it_code)) {

  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_INT32):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_FLOAT):
    emit_op4(iu, VM_SELECT32RR, value_reg(ret), value_reg(p),
             value_reg(tv), value_reg(fv));
    break;

  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_INT32):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_FLOAT):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_GLOBALVAR, IR_TYPE_POINTER):
    emit_op3(iu, VM_SELECT32RC, value_reg(ret), value_reg(p),
             value_reg(tv));
    emit_i32(iu, value_get_const32(iu, fv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_INT32):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_FLOAT):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_GLOBALVAR, IR_VC_REGFRAME, IR_TYPE_POINTER):
    emit_op3(iu, VM_SELECT32CR, value_reg(ret), value_reg(p),
             value_reg(fv));
    emit_i32(iu, value_get_const32(iu, tv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_INT32):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_FLOAT):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_GLOBALVAR, IR_VC_CONSTANT, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_GLOBALVAR, IR_TYPE_POINTER):
  case COMBINE3(IR_VC_GLOBALVAR, IR_VC_GLOBALVAR, IR_TYPE_POINTER):
    emit_op2(iu, VM_SELECT32CC, value_reg(ret), value_reg(p));
    emit_i32(iu, value_get_const32(iu, tv));
    emit_i32(iu, value_get_const32(iu, fv));
    break;



  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_INT16):
    emit_op4(iu, VM_SELECT16RR, value_reg(ret), value_reg(p),
             value_reg(tv), value_reg(fv));
    break;
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_INT16):
    emit_op3(iu, VM_SELECT16RC, value_reg(ret), value_reg(p),
             value_reg(tv));
    emit_i16(iu, value_get_const32(iu, fv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_INT16):
    emit_op3(iu, VM_SELECT16CR, value_reg(ret), value_reg(p),
             value_reg(fv));
    emit_i16(iu, value_get_const32(iu, tv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_INT16):
    emit_op2(iu, VM_SELECT16CC, value_reg(ret), value_reg(p));
    emit_i16(iu, value_get_const32(iu, tv));
    emit_i16(iu, value_get_const32(iu, fv));
    break;



  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_INT8):
    emit_op4(iu, VM_SELECT8RR, value_reg(ret), value_reg(p),
             value_reg(tv), value_reg(fv));
    break;
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_INT8):
    emit_op3(iu, VM_SELECT8RC, value_reg(ret), value_reg(p),
             value_reg(tv));
    emit_i8(iu, value_get_const32(iu, fv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_INT8):
    emit_op3(iu, VM_SELECT8CR, value_reg(ret), value_reg(p),
             value_reg(fv));
    emit_i8(iu, value_get_const32(iu, tv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_INT8):
    emit_op2(iu, VM_SELECT8CC, value_reg(ret), value_reg(p));
    emit_i8(iu, value_get_const32(iu, tv));
    emit_i8(iu, value_get_const32(iu, fv));
    break;


  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_INT64):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_REGFRAME, IR_TYPE_DOUBLE):
    emit_op4(iu, VM_SELECT64RR, value_reg(ret), value_reg(p),
             value_reg(tv), value_reg(fv));
    break;
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_INT64):
  case COMBINE3(IR_VC_REGFRAME, IR_VC_CONSTANT, IR_TYPE_DOUBLE):
    vm_align32(iu, 0);
    emit_op3(iu, VM_SELECT64RC, value_reg(ret), value_reg(p),
             value_reg(tv));
    emit_i64(iu, value_get_const64(iu, fv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_INT64):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_REGFRAME, IR_TYPE_DOUBLE):
    vm_align32(iu, 0);
    emit_op3(iu, VM_SELECT64CR, value_reg(ret), value_reg(p),
             value_reg(fv));
    emit_i64(iu, value_get_const64(iu, tv));
    break;
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_INT64):
  case COMBINE3(IR_VC_CONSTANT, IR_VC_CONSTANT, IR_TYPE_DOUBLE):
    vm_align32(iu, 1);
    emit_op2(iu, VM_SELECT64CC, value_reg(ret), value_reg(p));
    emit_i64(iu, value_get_const64(iu, tv));
    emit_i64(iu, value_get_const64(iu, fv));
    break;


  default:
    parser_error(iu, "Unable to emit select for %s class %d,%d",
                 type_str_index(iu, tv->iv_type),
                 tv->iv_class, fv->iv_class);
  }
}


/**
 *
 */
static void
emit_vmop(ir_unit_t *iu, ir_instr_call_t *ii)
{
  ir_function_t *f = value_function(iu, ii->callee);
  int vmop = f->if_vmop;
  assert(vmop != 0);
  emit_op(iu, vmop);

  if(ii->super.ii_ret_value < -1) {
    for(int i = 0; i < -ii->super.ii_ret_value; i++) {
      const ir_value_t *ret = value_get(iu, ii->super.ii_ret_values[i]);
      emit_i16(iu, value_reg(ret));
    }
  } else if(ii->super.ii_ret_value != -1) {
    const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
    emit_i16(iu, value_reg(ret));
  }

  for(int i = 0 ; i < ii->argc; i++) {
    const ir_value_t *iv = value_get(iu, ii->argv[i].value);
    emit_i16(iu, value_reg(iv));
  }
}


/**
 *
 */
static void
emit_vaarg(ir_unit_t *iu, ir_instr_unary_t *ii)
{
  const ir_value_t *val = value_get(iu, ii->value);
  int valreg;
  switch(val->iv_class) {
  case IR_VC_REGFRAME:
    valreg = value_reg(val);
    break;

  case IR_VC_CONSTANT:
  case IR_VC_GLOBALVAR:
    emit_op1(iu, VM_MOV32_C, 0);
    emit_i32(iu, value_get_const32(iu, val));
    valreg = 0;
    break;
  default:
    parser_error(iu, "bad vaarg class");
  }

  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_type_t *ty = type_get(iu, ret->iv_type);
  switch(ty->it_code) {
  case IR_TYPE_INT32:
  case IR_TYPE_POINTER:
  case IR_TYPE_INT1:
    emit_op(iu, VM_VAARG32);
    break;
  case IR_TYPE_DOUBLE:
  case IR_TYPE_INT64:
    emit_op(iu, VM_VAARG64);
    break;
  default:
    parser_error(iu, "Unable to emit vaarg() for type %s",
                 type_str(iu, ty));
  }

  emit_i16(iu, value_reg(ret));
  emit_i16(iu, valreg);
}


/**
 *
 */
static void
emit_mla(ir_unit_t *iu, ir_instr_ternary_t *ii)
{
  const ir_value_t *ret = value_get(iu, ii->super.ii_ret_value);
  const ir_type_t *ty = type_get(iu, ret->iv_type);
  const ir_value_t *a1 = value_get(iu, ii->arg1);
  const ir_value_t *a2 = value_get(iu, ii->arg2);
  const ir_value_t *a3 = value_get(iu, ii->arg3);
  switch(ty->it_code) {

  case IR_TYPE_INT32:
    emit_op4(iu, VM_MLA32, value_reg(ret),
             value_reg(a1), value_reg(a2), value_reg(a3));
    break;
  default:
    parser_error(iu, "Unable to emit mla() for type %s",
                 type_str(iu, ty));
  }
}


/**
 *
 */
static void
instr_emit(ir_unit_t *iu, ir_bb_t *bb, ir_function_t *f)
{
  ir_instr_t *ii;
  //  printf("=========== BB %s.%d\n", f->if_name, bb->ib_id);
  TAILQ_FOREACH(ii, &bb->ib_instrs, ii_link) {

    //    printf("EMIT INSTR: ");  instr_print(iu, ii, 1);  printf("\n");

#ifdef VMIR_VM_JIT
    if(ii->ii_jit) {
      int jitoffset;
      ii = jit_emit(iu, ii, &jitoffset, iu->iu_text_ptr - iu->iu_text_alloc + 6);
      if(jitoffset != -1) {
        emit_op(iu, VM_JIT_CALL);
        emit_i32(iu, jitoffset);
      }
      if(ii == NULL)
        return;
    }
#endif
    switch(ii->ii_class) {

    case IR_IC_RET:
      emit_ret(iu, (ir_instr_unary_t *)ii);
      break;
    case IR_IC_BINOP:
      emit_binop(iu, (ir_instr_binary_t *)ii);
      break;
    case IR_IC_LOAD:
      emit_load(iu, (ir_instr_load_t *)ii);
      break;
    case IR_IC_CMP2:
      emit_cmp2(iu, (ir_instr_binary_t *)ii);
      break;
    case IR_IC_BR:
      emit_br(iu, (ir_instr_br_t *)ii);
      break;
    case IR_IC_MOVE:
      emit_move(iu, (ir_instr_move_t *)ii);
      break;
    case IR_IC_STORE:
      emit_store(iu, (ir_instr_store_t *)ii);
      break;
    case IR_IC_LEA:
      emit_lea(iu, (ir_instr_lea_t *)ii);
      break;
    case IR_IC_CAST:
      emit_cast(iu, (ir_instr_unary_t *)ii);
      break;
    case IR_IC_CALL:
      emit_call(iu, (ir_instr_call_t *)ii, f);
      break;
    case IR_IC_SWITCH:
      emit_switch(iu, (ir_instr_switch_t *)ii);
      break;
    case IR_IC_ALLOCA:
      emit_alloca(iu, (ir_instr_alloca_t *)ii);
      break;
    case IR_IC_VAARG:
      emit_vaarg(iu, (ir_instr_unary_t *)ii);
      break;
    case IR_IC_SELECT:
      emit_select(iu, (ir_instr_select_t *)ii);
      break;
    case IR_IC_VMOP:
      emit_vmop(iu, (ir_instr_call_t *)ii);
      break;
    case IR_IC_STACKCOPY:
      emit_stackcopy(iu, (ir_instr_stackcopy_t *)ii);
      break;
    case IR_IC_STACKSHRINK:
      emit_stackshrink(iu, (ir_instr_stackshrink_t *)ii);
      break;
    case IR_IC_UNREACHABLE:
      emit_op(iu, VM_UNREACHABLE);
      break;
    case IR_IC_CMP_BRANCH:
      emit_cmp_branch(iu, (ir_instr_cmp_branch_t *)ii);
      break;
    case IR_IC_MLA:
      emit_mla(iu, (ir_instr_ternary_t *)ii);
      break;
    default:
      parser_error(iu, "Unable to emit instruction %d", ii->ii_class);
    }
  }
}


/**
 *
 */
static int16_t
bb_to_offset_delta(ir_function_t *f, int bbi, int off)
{
  ir_bb_t *bb = bb_find(f, bbi);
  if(bb == NULL) {
    printf("bb .%d not found\n", bbi);
    abort();
  }
  // The 2 is here because we need to compensate that the instruction
  // stream is past the opcode when the branch is executed
  int o = bb->ib_text_offset - (off + 2);

  assert(o >= INT16_MIN);
  assert(o <= INT16_MAX);
  return o;
}


/**
 * Finalize branch instructions.
 *
 * At time when we emitted them we didn't know where all basic blocks
 * started and ends, so we fix that now
 */
static void
branch_fixup(ir_unit_t *iu)
{
  ir_function_t *f = iu->iu_current_function;
  int x = VECTOR_LEN(&iu->iu_branch_fixups);
  int p;
  for(int i = 0; i < x; i++) {
    int off = VECTOR_ITEM(&iu->iu_branch_fixups, i);

    uint16_t *I = f->if_vm_text + off;
    switch(I[0]) {
    case VM_B:
      I[1] = bb_to_offset_delta(f, I[1], off);
      break;
    case VM_BCOND:
      I[2] = bb_to_offset_delta(f, I[2], off);
      I[3] = bb_to_offset_delta(f, I[3], off);
      break;
    case VM_EQ8_BR ... VM_SLE32_C_BR:
      I[1] = bb_to_offset_delta(f, I[1], off);
      I[2] = bb_to_offset_delta(f, I[2], off);
      break;
    case VM_JUMPTABLE:
      for(int j = 0; j < I[2]; j++)
        I[3 + j] = bb_to_offset_delta(f, I[3 + j], off);
      break;

    case VM_SWITCH8_BS:
      p = I[2];
      for(int j = 0; j < p + 1; j++)
        I[3 + p + j] = bb_to_offset_delta(f, I[3 + p + j], off);
      break;
    case VM_SWITCH32_BS:
      p = I[2];
      for(int j = 0; j < p + 1; j++)
        I[3 + p * 2 + j] = bb_to_offset_delta(f, I[3 + p * 2 + j], off);
      break;
    case VM_SWITCH64_BS:
      p = I[2];
      for(int j = 0; j < p + 1; j++)
        I[3 + p * 4 + j] = bb_to_offset_delta(f, I[3 + p * 4 + j], off);
      break;
    default:
      parser_error(iu, "Bad branch temporary opcode %d", I[0]);
    }
    I[0] = vm_resolve(I[0]);
  }
}



/**
 *
 */
static void
vm_emit_function(ir_unit_t *iu, ir_function_t *f)
{
  iu->iu_text_ptr = iu->iu_text_alloc;

  VECTOR_RESIZE(&iu->iu_branch_fixups, 0);
  VECTOR_RESIZE(&iu->iu_jit_vmcode_fixups, 0);
  VECTOR_RESIZE(&iu->iu_jit_vmbb_fixups, 0);
  VECTOR_RESIZE(&iu->iu_jit_branch_fixups, 0);

  ir_bb_t *ib;
  TAILQ_FOREACH(ib, &f->if_bbs, ib_link) {
    ib->ib_text_offset = iu->iu_text_ptr - iu->iu_text_alloc;
    if(iu->iu_debug_flags_func & VMIR_DBG_BB_INSTRUMENT) {
      emit_op(iu, VM_INSTRUMENT_COUNT);
      emit_i32(iu, VECTOR_LEN(&iu->iu_instrumentation));

      ir_instr_t *i;
      int num_instructions = 0;
      TAILQ_FOREACH(i, &ib->ib_instrs, ii_link)
        num_instructions++;

      ir_instrumentation_t ii = {f, ib->ib_id, num_instructions, 0};
      VECTOR_PUSH_BACK(&iu->iu_instrumentation, ii);
    }
    instr_emit(iu, ib, f);
  }

  f->if_vm_text_size = iu->iu_text_ptr - iu->iu_text_alloc;
  f->if_vm_text = malloc(f->if_vm_text_size);
  memcpy(f->if_vm_text, iu->iu_text_alloc, f->if_vm_text_size);

  branch_fixup(iu);
#ifdef VMIR_VM_JIT
  jit_branch_fixup(iu, f);
#endif
}


/**
 *
 */
static int
vm_function_call(ir_unit_t *iu, ir_function_t *f, void *out, ...)
{
  va_list ap;
  const ir_type_t *it = &VECTOR_ITEM(&iu->iu_types, f->if_type);
  assert(it->it_code == IR_TYPE_FUNCTION);
  uint32_t u32;
  int argpos = 0;
  void *rf = iu->iu_mem;
  va_start(ap, out);

  argpos += it->it_function.num_parameters * sizeof(uint32_t);

  void *rfa = rf + argpos;

  for(int i = 0; i < it->it_function.num_parameters; i++) {
    const ir_type_t *arg = &VECTOR_ITEM(&iu->iu_types,
                                        it->it_function.parameters[i]);
    switch(arg->it_code) {
    case IR_TYPE_INT8:
    case IR_TYPE_INT16:
    case IR_TYPE_INT32:
    case IR_TYPE_POINTER:
      argpos -= 4;
      u32 = va_arg(ap, int);
      *(uint32_t *)(rf + argpos) = u32;
      break;

    default:
      fprintf(stderr, "Unable to encode argument %d (%s) in call to %s\n",
              i, type_str(iu, arg), f->if_name);
      return 0;
    }
  }

  int r = setjmp(iu->iu_err_jmpbuf);
  if(r) {
    switch(r) {
    case VM_STOP_EXIT:
      printf("Program exit: 0x%x\n", iu->iu_exit_code);
      break;
    case VM_STOP_ABORT:
      printf("Program abort\n");
      break;
    case VM_STOP_UNREACHABLE:
      printf("Unreachable instruction\n");
      break;
    case VM_STOP_BAD_INSTRUCTION:
      printf("Bad instruction\n");
      break;
    case VM_STOP_BAD_FUNCTION:
      printf("Bad function %d\n", iu->iu_exit_code);
      break;
    }
    return r;
  }

  vm_exec(f->if_vm_text, rfa, iu, out, iu->iu_alloca_ptr, -1);
  return r;
}


