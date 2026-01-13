#-------------------------------------------------------------------------------
#   m65c02_decoder.py
#   Generate instruction decoder for m65c02.h emulator.
#   WIP! But runs 65C02_extended_opcodes_test.bin just fine.
#-------------------------------------------------------------------------------
import templ

INOUT_PATH = '../chips/m65c02.h'

# flag bits
CF = (1<<0)
ZF = (1<<1)
IF = (1<<2)
DF = (1<<3)
BF = (1<<4)
XF = (1<<5)
VF = (1<<6)
NF = (1<<7)

def flag_name(f):
    if f == CF: return 'C'
    elif f == ZF: return 'Z'
    elif f == IF: return 'I'
    elif f == DF: return 'D'
    elif f == BF: return 'B'
    elif f == XF: return 'X'
    elif f == VF: return 'V'
    elif f == NF: return 'N'

def branch_name(m, v):
    if m == NF:
        return 'BPL' if v==0 else 'BMI'
    elif m == VF:
        return 'BVC' if v==0 else 'BVS'
    elif m == CF:
        return 'BCC' if v==0 else 'BCS'
    elif m == ZF:
        return 'BNE' if v==0 else 'BEQ'

# addressing mode constants
A____ = 0       # no addressing mode
A_IMM = 1       # immediate
A_ZER = 2       # zero-page
A_ZPX = 3       # zp,X
A_ZPY = 4       # zp,Y
A_ABS = 5       # abs
A_ABX = 6       # abs,X
A_ABY = 7       # abs,Y
A_IAI = 8       # (abs,X) "indexed absolute indirect" (65c02 jmp)
A_IDX = 9       # (zp,X)
A_IDY = 10      # (zp),Y
A_IND = 11      # (zp)
A_JMP = 12      # special JMP abs
A_JSR = 13      # special JSR abs
A_INV = 14      # an invalid instruction

# addressing mode strings
addr_mode_str = ['', '#', 'zp', 'zp,X', 'zp,Y', 'abs', 'abs,X', 'abs,Y', '(abs,X)', '(zp,X)', '(zp),Y', '(zp)', '', '', 'INVALID']

# memory access modes
M___ = 0        # no memory access
M_R_ = 1        # read access
M__W = 2        # write access
M_RW = 3        # read-modify-write

# addressing-modes and memory accesses for each instruction ([cc][bbb][aaa])
ops = [
    # cc = 00
    [
        # ---         BIT          JMP          JMP()        STY          LDY          CPY          CPX
        [[A____,M___],[A_JSR,M_R_],[A____,M_R_],[A____,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_ZER,M_RW],[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M__W],[A_ZER,M__W],[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M_R_]],
        [[A____,M__W],[A____,M___],[A____,M__W],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___]],
        [[A_ABS,M_RW],[A_ABS,M_R_],[A_JMP,M_R_],[A_JMP,M_R_],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_]],
        [[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],  # relative branches
        [[A_ZER,M_RW],[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M__W],[A_ZPX,M__W],[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M_R_]],
        [[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___]],
        [[A_ABS,M_RW],[A_ABX,M_R_],[A_ABX,M_R_],[A_IAI,M_R_],[A_ABS,M__W],[A_ABX,M_R_],[A_ABS,M_R_],[A_ABS,M_R_]]
    ],
    # cc = 01
    [
        # ORA         AND          EOR          ADC          STA          LDA          CMP          SBC
        [[A_IDX,M_R_],[A_IDX,M_R_],[A_IDX,M_R_],[A_IDX,M_R_],[A_IDX,M__W],[A_IDX,M_R_],[A_IDX,M_R_],[A_IDX,M_R_]],
        [[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M__W],[A_ZER,M_R_],[A_ZER,M_R_],[A_ZER,M_R_]],
        [[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_]],
        [[A_IDY,M_R_],[A_IDY,M_R_],[A_IDY,M_R_],[A_IDY,M_R_],[A_IDY,M__W],[A_IDY,M_R_],[A_IDY,M_R_],[A_IDY,M_R_]],
        [[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M__W],[A_ZPX,M_R_],[A_ZPX,M_R_],[A_ZPX,M_R_]],
        [[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M__W],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_]],
        [[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M__W],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_]]
    ],
    # cc = 02
    [
        # ASL         ROL          LSR          ROR          STX          LDX          DEC          INC
        [[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M__W],[A_ZER,M_R_],[A_ZER,M_RW],[A_ZER,M_RW]],
        [[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___],[A____,M___]],
        [[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_RW],[A_ABS,M_RW]],
        [[A_IND,M_R_],[A_IND,M_R_],[A_IND,M_R_],[A_IND,M_R_],[A_IND,M__W],[A_IND,M_R_],[A_IND,M_R_],[A_IND,M_R_]],
        [[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPY,M__W],[A_ZPY,M_R_],[A_ZPX,M_RW],[A_ZPX,M_RW]],
        [[A____,M_R_],[A____,M___],[A____,M__W],[A____,M_R_],[A____,M___],[A____,M___],[A____,M__W],[A____,M_R_]],
        [[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M__W],[A_ABY,M_R_],[A_ABX,M_RW],[A_ABX,M_RW]]
    ],
    # cc = 03 - NOTE this isn't really used but overridden at the beginning of enc_op
    [
        [[A_IDX,M_RW],[A_IDX,M_RW],[A_IDX,M_RW],[A_IDX,M_RW],[A_IDX,M__W],[A_IDX,M_R_],[A_IDX,M_RW],[A_IDX,M_RW]],
        [[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M_RW],[A_ZER,M__W],[A_ZER,M_R_],[A_ZER,M_RW],[A_ZER,M_RW]],
        [[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_RW],[A_ABS,M_RW]],
        [[A_IDY,M_RW],[A_IDY,M_RW],[A_IDY,M_RW],[A_IDY,M_RW],[A_IDY,M_RW],[A_IDY,M_R_],[A_IDY,M_RW],[A_IDY,M_RW]],
        [[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPX,M_RW],[A_ZPY,M__W],[A_ZPY,M_R_],[A_ZPX,M_RW],[A_ZPX,M_RW]],
        [[A_ABY,M_RW],[A_ABY,M_RW],[A_ABY,M_RW],[A_ABY,M_RW],[A_ABY,M__W],[A_ABY,M_R_],[A_ABY,M_RW],[A_ABY,M_RW]],
        [[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABY,M__W],[A_ABY,M_R_],[A_ABX,M_RW],[A_ABX,M_RW]]
    ]
]

class opcode:
    def __init__(self, op):
        self.code = op
        self.cmt = None
        self.i = 0
        self.src = [None] * 8
    def t(self, src):
        self.src[self.i] = src
        self.i += 1
    def ta(self, src):
        self.src[self.i-1] += src

#-------------------------------------------------------------------------------
#   output a src line
#
out_lines = ''
def l(s) :
    global out_lines
    out_lines += s + '\n'

#-------------------------------------------------------------------------------
def write_op(op):
    if not op.cmt:
        op.cmt = '???'
    l('    /* {} */'.format(op.cmt if op.cmt else '???'))
    for t in range(0, 8):
        if t < op.i:
            l('        case (0x{:02X}<<3)|{}: {}break;'.format(op.code, t, op.src[t]))
        else:
            l('        case (0x{:02X}<<3)|{}: assert(false);break;'.format(op.code, t))

#-------------------------------------------------------------------------------
def cmt(o,cmd):
    cc = o.code & 3
    bbb = (o.code>>2) & 7
    aaa = (o.code>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    o.cmt = cmd;
    if addr_mode != '':
        o.cmt += ' '+addr_mode_str[addr_mode]

#-------------------------------------------------------------------------------
def u_cmt(o,cmd):
    cmt(o,cmd)
    o.cmt += ' (undoc)'

#-------------------------------------------------------------------------------
def invalid_opcode(op):
    cc = op & 3
    bbb = (op>>2) & 7
    aaa = (op>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    return addr_mode == A_INV

#-------------------------------------------------------------------------------
def enc_addr(op, addr_mode, mem_access):
    if addr_mode == A____:
        # no addressing, this still puts the PC on the address bus without 
        # incrementing the PC
        op.t('_SA(c->PC);')
    elif addr_mode == A_IMM:
        # immediate mode
        op.t('_SA(c->PC++);')
    elif addr_mode == A_ZER:
        # zero page
        op.t('_SA(c->PC++);')
        op.t('_SA(_GD());')
    elif addr_mode == A_ZPX:
        # zero page + X
        op.t('_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_SA((c->AD+c->X)&0x00FF);')
    elif addr_mode == A_ZPY:
        # zero page + Y
        op.t('_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_SA((c->AD+c->Y)&0x00FF);')
    elif addr_mode == A_ABS:
        # absolute
        op.t('_SA(c->PC++);')
        op.t('_SA(c->PC++);c->AD=_GD();')
        op.t('_SA((_GD()<<8)|c->AD);')
    elif addr_mode == A_ABX:
        # TODO 65C02 reads opcode address on ind page boundary cross
        # absolute + X
        # this needs to check if a page boundary is crossed, which costs
        # and additional cycle, but this early-out only happens when the
        # instruction doesn't need to write back to memory
        op.t('_SA(c->PC++);')
        op.t('_SA(c->PC++);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->X)&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+c->X)>>8)))&1;')
        op.t('_SA(c->AD+c->X);')
    elif addr_mode == A_ABY:
        # absolute + Y
        # same page-boundary-crossed special case as absolute+X
        op.t('_SA(c->PC++);')
        op.t('_SA(c->PC++);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;')
        op.t('_SA(c->AD+c->Y);')
    elif addr_mode == A_IND:
        # (zp)
        op.t('_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_SA((c->AD+1)&0xFF);c->AD=_GD();')
        op.t('_SA((_GD()<<8)|c->AD);')
    elif addr_mode == A_IDX:
        # (zp,X)
        op.t('_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('c->AD=(c->AD+c->X)&0xFF;_SA(c->AD);')
        op.t('_SA((c->AD+1)&0xFF);c->AD=_GD();')
        op.t('_SA((_GD()<<8)|c->AD);')
    elif addr_mode == A_IDY:
        # (zp),Y
        # same page-boundary-crossed special case as absolute+X
        op.t('_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_SA((c->AD+1)&0xFF);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+c->Y)&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+c->Y)>>8)))&1;')
        op.t('_SA(c->AD+c->Y);')
    elif addr_mode == A_JMP:
        # jmp is completely handled in instruction decoding
        pass
    elif addr_mode == A_JSR:
        # jsr is completely handled in instruction decoding 
        pass
    else:
        # invalid instruction
        pass

#-------------------------------------------------------------------------------
def i_brk(o):
    cmt(o, 'BRK')  # TODO 65C02 doesn't swallow BRKs on NMI
    o.t('if(0==(c->brk_flags&(M6502_BRK_IRQ|M6502_BRK_NMI))){c->PC++;}_SAD(0x0100|c->S--,c->PC>>8);if(0==(c->brk_flags&M6502_BRK_RESET)){_WR();}')
    o.t('_SAD(0x0100|c->S--,c->PC);if(0==(c->brk_flags&M6502_BRK_RESET)){_WR();}')
    o.t('_SAD(0x0100|c->S--,c->P|M6502_XF);if(c->brk_flags&M6502_BRK_RESET){c->AD=0xFFFC;}else{_WR();if(c->brk_flags&M6502_BRK_NMI){c->AD=0xFFFA;}else{c->AD=0xFFFE;}}')
    o.t('_SA(c->AD++);c->P&=~M6502_DF;c->P|=(M6502_IF|M6502_BF);c->brk_flags=0; /* RES/NMI hijacking */')
    o.t('_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_nop(o):
    cmt(o,'NOP')
    o.t('')

#-------------------------------------------------------------------------------
def u_nop(o):
    u_cmt(o,'NOP')
    o.t('')

#-------------------------------------------------------------------------------
def i_lda(o):
    cmt(o,'LDA')
    o.t('c->A=_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_ldx(o):
    cmt(o,'LDX')
    o.t('c->X=_GD();_NZ(c->X);')

#-------------------------------------------------------------------------------
def i_ldy(o):
    cmt(o,'LDY')
    o.t('c->Y=_GD();_NZ(c->Y);')

#-------------------------------------------------------------------------------
def i_ldd(o):
    cmt(o,'LDD')
    o.t('')

#-------------------------------------------------------------------------------
def u_lax(o):
    u_cmt(o,'LAX')
    o.t('c->A=c->X=_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def x_lxa(o):
    # undocumented LXA
    # and immediate byte with A, then load X with A
    u_cmt(o,'LXA')
    o.t('c->A=c->X=(c->A|0xEE)&_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_sta(o):
    cmt(o,'STA')
    o.ta('_SD(c->A);_WR();')

#-------------------------------------------------------------------------------
def i_stx(o):
    cmt(o,'STX')
    o.ta('_SD(c->X);_WR();')

#-------------------------------------------------------------------------------
def i_sty(o):
    cmt(o,'STY')
    o.ta('_SD(c->Y);_WR();')

#-------------------------------------------------------------------------------
def i_stz(o):
    cmt(o,'STZ')
    o.ta('_SD(0);_WR();')

#-------------------------------------------------------------------------------
def u_sax(o):
    u_cmt(o,'SAX')
    o.ta('_SD(c->A&c->X);_WR();')

#-------------------------------------------------------------------------------
def i_tax(o):
    cmt(o,'TAX')
    o.t('c->X=c->A;_NZ(c->X);')

#-------------------------------------------------------------------------------
def i_tay(o):
    cmt(o,'TAY')
    o.t('c->Y=c->A;_NZ(c->Y);')

#-------------------------------------------------------------------------------
def i_txa(o):
    cmt(o,'TXA')
    o.t('c->A=c->X;_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_tya(o):
    cmt(o,'TYA')
    o.t('c->A=c->Y;_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_txs(o):
    cmt(o,'TXS')
    o.t('c->S=c->X;')

#-------------------------------------------------------------------------------
def i_tsx(o):
    cmt(o,'TSX')
    o.t('c->X=c->S;_NZ(c->X);')

#-------------------------------------------------------------------------------
def i_php(o):
    cmt(o,'PHP')
    o.t('_SAD(0x0100|c->S--,c->P|M6502_XF);_WR();')

#-------------------------------------------------------------------------------
def i_plp(o):
    cmt(o,'PLP')
    o.t('_SA(0x0100|c->S++);')   # read junk byte from current SP
    o.t('_SA(0x0100|c->S);')     # read actual byte  
    o.t('c->P=(_GD()|M6502_BF)&~M6502_XF;');

#-------------------------------------------------------------------------------
def i_ph(o, f):
    cmt(o,'PH'+f)
    o.t('_SAD(0x0100|c->S--,c->'+f+');_WR();')

#-------------------------------------------------------------------------------
def i_pl(o, f):
    cmt(o,'PL'+f)
    o.t('_SA(0x0100|c->S++);') # read junk byte from current SP
    o.t('_SA(0x0100|c->S);')   # read actual byte
    o.t('c->'+f+'=_GD();_NZ(c->'+f+');')

#-------------------------------------------------------------------------------
def i_se(o, f):
    cmt(o,'SE'+flag_name(f))
    o.t('c->P|='+hex(f)+';')

#-------------------------------------------------------------------------------
def i_cl(o, f):
    cmt(o,'CL'+flag_name(f))
    o.t('c->P&=~'+hex(f)+';')

#-------------------------------------------------------------------------------
def i_br(o, m, v):
    cmt(o,branch_name(m,v))
    # if branch not taken?
    o.t('_SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&'+hex(m)+')!='+hex(v)+'){_FETCH();};')
    # branch taken: shortcut if page not crossed, 'branchquirk' interrupt fix
    o.t('_SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};')
    # page crossed extra cycle:
    o.t('c->PC=c->AD;')

#-------------------------------------------------------------------------------
def i_bra(o):
    cmt(o,'BRA')
    o.t('_SA(c->PC);c->AD=c->PC+(int8_t)_GD();')
    # branch taken: shortcut if page not crossed, 'branchquirk' interrupt fix
    o.t('_SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};')
    # page crossed extra cycle:
    o.t('c->PC=c->AD;')

#-------------------------------------------------------------------------------
def i_bbX(o, s, i):
    cmt(o,f'BB{s}{i}')
    # fetch branch target
    o.t('c->AD=_GD();_SA(c->PC++);')
    if s == 'S':
      o.t('if(c->AD&(1<<'+str(i)+')){c->PC=c->PC+(int8_t)_GD();_FETCH();};')
    else:
      o.t('if(!(c->AD&(1<<'+str(i)+'))){c->PC=c->PC+(int8_t)_GD();_FETCH();};')

#-------------------------------------------------------------------------------
def i_Xmb(o, s, i):
    cmt(o,f'{s}MB{i}')
    if s == 'S':
      o.t(f'_SD(_GD()|(1<<{i}));_WR();')
    else:
      o.t(f'_SD(_GD()&(~(1<<{i})));_WR();')

#-------------------------------------------------------------------------------
def i_jmp(o):
    cmt(o,'JMP')
    o.t('_SA(c->PC++);')
    o.t('_SA(c->PC++);c->AD=_GD();')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jmpi(o):
    cmt(o,'JMPI')
    o.t('_SA(c->PC++);')
    o.t('_SA(c->PC++);c->AD=_GD();')
    o.t('c->AD|=_GD()<<8;_SA(c->AD);')
    o.t('_SA(c->AD+1);c->AD=_GD();')  # fixed the NMOS flaw
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jmpix(o):
    cmt(o,'JMPIX')
    o.t('_SA(c->PC++);')
    o.t('_SA(c->PC++);c->AD=_GD();')
    o.t('c->AD|=_GD()<<8;c->AD+=c->X;_SA(c->AD);')
    o.t('_SA(c->AD+1);c->AD=_GD();')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jsr(o):
    cmt(o,'JSR')
    # read low byte of target address
    o.t('_SA(c->PC++);')
    # put SP on addr bus, next cycle is a junk read
    o.t('_SA(0x0100|c->S);c->AD=_GD();')
    # write PC high byte to stack
    o.t('_SAD(0x0100|c->S--,c->PC>>8);_WR();')
    # write PC low byte to stack
    o.t('_SAD(0x0100|c->S--,c->PC);_WR();')
    # load target address high byte
    o.t('_SA(c->PC);')
    # load PC and done
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_rts(o):
    cmt(o,'RTS')
    # put SP on stack and do a junk read
    o.t('_SA(0x0100|c->S++);')
    # load return address low byte from stack
    o.t('_SA(0x0100|c->S++);')
    # load return address high byte from stack
    o.t('_SA(0x0100|c->S);c->AD=_GD();')
    # put return address in PC, this is one byte before next op, do junk read from PC
    o.t('c->PC=(_GD()<<8)|c->AD;_SA(c->PC++);')
    # next tick is opcode fetch
    o.t('');

#-------------------------------------------------------------------------------
def i_rti(o):
    cmt(o,'RTI')
    # put SP on stack and do a junk read
    o.t('_SA(0x0100|c->S++);')
    # load processor status flag from stack
    o.t('_SA(0x0100|c->S++);')
    # load return address low byte from stack
    o.t('_SA(0x0100|c->S++);c->P=(_GD()|M6502_BF)&~M6502_XF;')
    # load return address high byte from stack
    o.t('_SA(0x0100|c->S);c->AD=_GD();')
    # update PC (which is already placed on the right return-to instruction)
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_ora(o):
    cmt(o,'ORA')
    o.t('c->A|=_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_and(o):
    cmt(o,'AND')
    o.t('c->A&=_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_eor(o):
    cmt(o,'EOR')
    o.t('c->A^=_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_adc(o):
    cmt(o,'ADC')
    o.t('_m6502_adc(c,_GD());if(c->P&M6502_DF){c->P=((c->P&~(M6502_NF|M6502_ZF))|((c->A&0xFF)?(c->A&M6502_NF):M6502_ZF));};')

#-------------------------------------------------------------------------------
def i_sbc(o):
    cmt(o,'SBC')
    o.t('_m6502_sbc(c,_GD());if(c->P&M6502_DF){c->P=((c->P&~(M6502_NF|M6502_ZF))|((c->A&0xFF)?(c->A&M6502_NF):M6502_ZF));};')

#-------------------------------------------------------------------------------
def u_sbc(o):
    u_cmt(o,'SBC')
    o.t('_m6502_sbc(c,_GD());')

#-------------------------------------------------------------------------------
def i_cmp(o):
    cmt(o,'CMP')
    o.t('_m6502_cmp(c, c->A, _GD());')

#-------------------------------------------------------------------------------
def i_cpx(o):
    cmt(o,'CPX')
    o.t('_m6502_cmp(c, c->X, _GD());')

#-------------------------------------------------------------------------------
def i_cpy(o):
    cmt(o,'CPY')
    o.t('_m6502_cmp(c, c->Y, _GD());')

#-------------------------------------------------------------------------------
def u_dcp(o):
    # undocumented 'decrement and compare'
    u_cmt(o,'DCP')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD--;_NZ(c->AD);_SD(c->AD);_m6502_cmp(c, c->A, c->AD);_WR();')

#-------------------------------------------------------------------------------
def x_sbx(o):
    u_cmt(o,'SBX')
    o.t('_m6502_sbx(c, _GD());')

#-------------------------------------------------------------------------------
def i_dec(o):
    cmt(o,'DEC')
    o.t('c->AD=_GD();')  # CMOS: no dummy write
    o.t('c->AD--;_NZ(c->AD);_SD(c->AD);_WR();')

#-------------------------------------------------------------------------------
def i_inc(o):
    cmt(o,'INC')
    o.t('c->AD=_GD();')  # CMOS: no dummy write
    o.t('c->AD++;_NZ(c->AD);_SD(c->AD);_WR();')

#-------------------------------------------------------------------------------
def i_deca(o):
    cmt(o,'DEC A')
    o.t('c->A--;_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_dex(o):
    cmt(o,'DEX')
    o.t('c->X--;_NZ(c->X);')

#-------------------------------------------------------------------------------
def i_dey(o):
    cmt(o,'DEY')
    o.t('c->Y--;_NZ(c->Y);')

#-------------------------------------------------------------------------------
def i_inca(o):
    cmt(o,'INC A')
    o.t('c->A++;_NZ(c->A);')

#-------------------------------------------------------------------------------
def i_inx(o):
    cmt(o,'INX')
    o.t('c->X++;_NZ(c->X);')

#-------------------------------------------------------------------------------
def i_iny(o):
    cmt(o,'INY')
    o.t('c->Y++;_NZ(c->Y);')

#-------------------------------------------------------------------------------
def u_isb(o):
    # undocumented INC+SBC instruction
    u_cmt(o,'ISB')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD++;_SD(c->AD);_m6502_sbc(c,c->AD);_WR();')

#-------------------------------------------------------------------------------
def i_asl(o):
    cmt(o,'ASL')
    o.t('c->AD=_GD();_WR();')
    o.t('_SD(_m6502_asl(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_asla(o):
    cmt(o,'ASLA')
    o.t('c->A=_m6502_asl(c,c->A);')

#-------------------------------------------------------------------------------
def i_lsr(o):
    cmt(o,'LSR')
    o.t('c->AD=_GD();_WR();')
    o.t('_SD(_m6502_lsr(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_lsra(o):
    cmt(o,'LSRA')
    o.t('c->A=_m6502_lsr(c,c->A);')

#-------------------------------------------------------------------------------
def u_slo(o):
    # undocumented ASL+OR
    u_cmt(o,'SLO')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD=_m6502_asl(c,c->AD);_SD(c->AD);c->A|=c->AD;_NZ(c->A);_WR();')

#-------------------------------------------------------------------------------
def x_asr(o):
    # undocumented AND+LSR
    u_cmt(o, 'ASR')
    o.t('c->A&=_GD();c->A=_m6502_lsr(c,c->A);')

#-------------------------------------------------------------------------------
def u_sre(o):
    # undocumented LSR+EOR
    u_cmt(o,'SRE')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD=_m6502_lsr(c,c->AD);_SD(c->AD);c->A^=c->AD;_NZ(c->A);_WR();')

#-------------------------------------------------------------------------------
def i_rol(o):
    cmt(o,'ROL')
    o.t('c->AD=_GD();_WR();')
    o.t('_SD(_m6502_rol(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_rola(o):
    cmt(o,'ROLA')
    o.t('c->A=_m6502_rol(c,c->A);')

#-------------------------------------------------------------------------------
def u_rla(o):
    # uncodumented ROL+AND
    u_cmt(o,'RLA')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD=_m6502_rol(c,c->AD);_SD(c->AD);c->A&=c->AD;_NZ(c->A);_WR();')

#-------------------------------------------------------------------------------
def i_ror(o):
    cmt(o,'ROR')
    o.t('c->AD=_GD();_WR();')
    o.t('_SD(_m6502_ror(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_rora(o):
    cmt(o,'RORA')
    o.t('c->A=_m6502_ror(c,c->A);')

#-------------------------------------------------------------------------------
def u_rra(o):
    # undocumented ROR+ADC
    u_cmt(o,'RRA')
    o.t('c->AD=_GD();_WR();')
    o.t('c->AD=_m6502_ror(c,c->AD);_SD(c->AD);_m6502_adc(c,c->AD);_WR();')

#-------------------------------------------------------------------------------
def x_arr(o):
    # undocumented AND+ROR
    u_cmt(o,'ARR')
    o.t('c->A&=_GD();_m6502_arr(c);')

#-------------------------------------------------------------------------------
def x_ane(o):
    # undocumented ANE
    u_cmt(o,'ANE')
    o.t('c->A=(c->A|0xEE)&c->X&_GD();_NZ(c->A);')

#-------------------------------------------------------------------------------
def x_sha(o):
    # undocumented SHA
    #  stores the result of A AND X AND the high byte of the target address of
    #  the operand +1 in memory
    #
    u_cmt(o,'SHA')
    o.ta('_SD(c->A&c->X&(uint8_t)((_GA()>>8)+1));_WR();')

#-------------------------------------------------------------------------------
def x_shx(o):
    # undocumented SHX
    # AND X register with the high byte of the target address of the 
    # argument + 1. Store the result in memory.
    #
    u_cmt(o, 'SHX')
    o.ta('_SD(c->X&(uint8_t)((_GA()>>8)+1));_WR();')

#-------------------------------------------------------------------------------
def x_shy(o):
    # undocumented SHX
    # AND Y register with the high byte of the target address of the
    # argument + 1. Store the result in memory.
    #
    u_cmt(o, 'SHY')
    o.ta('_SD(c->Y&(uint8_t)((_GA()>>8)+1));_WR();')

#-------------------------------------------------------------------------------
def x_shs(o):
    # undocumented SHS
    # AND X register with accumulator and store result in stack pointer, then
    # AND stack pointer with the high byte of the target address of the
    # argument + 1. Store result in memory.
    #
    u_cmt(o, 'SHS')
    o.ta('c->S=c->A&c->X;_SD(c->S&(uint8_t)((_GA()>>8)+1));_WR();')

#-------------------------------------------------------------------------------
def x_anc(o):
    # undocumented ANC
    # AND byte with accumulator. If result is negative then carry is set.
    #
    u_cmt(o, 'ANC')
    o.t('c->A&=_GD();_NZ(c->A);if(c->A&0x80){c->P|=M6502_CF;}else{c->P&=~M6502_CF;}')

#-------------------------------------------------------------------------------
def x_las(o):
    # undocumented LAS
    # AND memory with stack pointer, transfer result to accumulator, X
    # register and stack pointer.
    #
    u_cmt(o, 'LAS')
    o.t('c->A=c->X=c->S=_GD()&c->S;_NZ(c->A);')

#-------------------------------------------------------------------------------
def x_jam(o):
    # undocumented JAM, next opcode byte read, data and addr bus set to all 1, execution stops
    u_cmt(o, 'JAM')
    o.t('_SA(c->PC);')
    o.t('_SAD(0xFFFF,0xFF);c->IR--;')

#-------------------------------------------------------------------------------
def i_trb(o):  # TODO check function and cycles
    cmt(o,'TRB')
    o.t('c->AD=_GD();')
    o.t('if(c->AD&c->A){c->P&=~M6502_ZF;}else{c->P|=M6502_ZF;};_SD(c->AD&~c->A);_WR();')

#-------------------------------------------------------------------------------
def i_tsb(o):  # TODO check function and cycles
    cmt(o,'TSB')
    o.t('c->AD=_GD();')
    o.t('if(c->AD&c->A){c->P&=~M6502_ZF;}else{c->P|=M6502_ZF;};_SD(c->AD|c->A);_WR();')

#-------------------------------------------------------------------------------
def i_bit(o):
    cmt(o,'BIT')
    if o.code == 0x89:
      # on 65C02, BIT #xx doesn't touch N and V
      o.t('c->P&=~M6502_ZF;if(!(c->A&_GD())){c->P|=M6502_ZF;};')
    else:
      o.t('_m6502_bit(c,_GD());')

#-------------------------------------------------------------------------------
def enc_op(op):
    o = opcode(op)
    if (op&0xf) == 7:
        # RBM0-7/SMB0-7
        enc_addr(o, A_ZER, M_RW)
        if (op>>4) < 8:
          i_Xmb(o, "R", op>>4)
          o.cmt = f"RMB{op>>4} zp"
        else:
          i_Xmb(o, "S", (op>>4) - 8)
          o.cmt = f"SMB{(op>>4) - 8} zp"
        o.t('_FETCH();')
        return o
    if (op&0xf) == 0xf:
        # BBR0-7/BBS0-7
        enc_addr(o, A_ZER, M_R_)
        if (op>>4) < 8:
          i_bbX(o, "R", op>>4)
          o.cmt = f"BBR{op>>4} zp,rel"
        else:
          i_bbX(o, "S", (op>>4) - 8)
          o.cmt = f"BBS{(op>>4) - 8} zp,rel"
        o.ta('_FETCH();')
        return o
    if (op&3) == 3:
        enc_addr(o, A____, M___)
        i_nop(o)
        o.ta('_FETCH();')
        return o
    if invalid_opcode(op):
        x_jam(o);
        return o

    # decode the opcode byte
    cc = op & 3
    bbb = (op>>2) & 7
    aaa = (op>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    mem_access = ops[cc][bbb][aaa][1]
    # addressing mode
    enc_addr(o, addr_mode, mem_access);
    # actual instruction
    # XXX https://github.com/visrealm/vrEmu6502/blob/37d5dab79317468a7031fc68075d8529aea35a1a/src/vrEmu6502.c#L2061
    # XXX https://en.wikipedia.org/wiki/WDC_65C02#Bit_manipulation_instructions
    # XXX https://www.westerndesigncenter.com/wdc/documentation/w65c02s.pdf
    # TODO decimal ops flags: one additional cycle
    # TODO int while BRK decoding: execute BRK then int (instead of ignore BRK)
    # TODO WAI instruction, bidirectional RDY line
    # TODO RMW abs ind same page: six cycles (instead of seven)
    #      op name     a b c
    #      80 bra      4 0 0
    #      02 ldd_imm  0 0 2  load and discard
    #      12 ora zpi  0 4 2
    #      22 ldd imm  1 0 2
    #      32 and zpi  1 4 2
    #      42 ldd imm  2 0 2
    #      52 eor zpi  2 4 2
    #      62 ldd imm  3 0 2
    #      72 adc zpi  3 4 2
    #      82 ldd imm  4 0 2
    #      92 sta zpi  4 4 2
    #      a2 ldx imm  5 0 2
    #      b2 lda zpi  5 4 2
    #      c2 ldd_imm  6 0 2
    #      d2 cmp zpi  6 4 2
    #      e2 ldd imm  7 0 2
    #      f2 sbc zpi  7 4 2
    #      _3 unnop
    #      04 tsb zp   0 1 0  test and set bit
    #      14 trb zp   0 5 0  test and clear bit
    #      34 bit zpx  1 5 0
    #      44 ldd zp   2 1 0
    #      54 ldd zpx  2 5 0
    #      64 stz zp   3 1 0  store zero
    #      74 stz zpx  3 5 0
    #      D4 ldd zpx  6 5 0
    #      F4 ldd zpx  7 5 0
    #      _7 RMB/SMB
    #      89 bit imm  4 2 1
    #      1A inc acc  0 6 2
    #      3A dec acc  1 6 2
    #      5A phy imp  2 6 2  push y
    #      7A ply imp  3 6 2  pull y
    #      DA phx imp  6 6 2
    #      FA plx imp  7 6 2
    #      _B unnop
    #      0C tsb ab   0 3 0
    #      1C trb ab   0 7 0
    #      3C bit abx  1 7 0
    #      5C ldd ab   2 7 0
    #      7C jmp indx 3 7 0
    #      9C stz ab   4 7 0
    #      DC ldd ab   6 7 0
    #      FC ldd ab   7 7 0
    #      9E stz abx  4 7 2
    #      _F BBR/BBS

    if cc == 0:
        if aaa == 0:
            if bbb == 0:        i_brk(o)
            elif bbb == 1:      i_tsb(o)
            elif bbb == 2:      i_php(o)
            elif bbb == 3:      i_tsb(o)
            elif bbb == 4:      i_br(o, NF, 0)  # BPL
            elif bbb == 5:      i_trb(o)
            elif bbb == 6:      i_cl(o, CF)
            elif bbb == 7:      i_trb(o)
        elif aaa == 1:
            if bbb == 0:        i_jsr(o)
            elif bbb == 2:      i_plp(o)
            elif bbb == 4:      i_br(o, NF, NF) # BMI
            elif bbb == 5:      i_bit(o)
            elif bbb == 6:      i_se(o, CF)
            elif bbb == 7:      i_bit(o)
            else:               i_bit(o)
        elif aaa == 2:
            if bbb == 0:        i_rti(o)
            elif bbb == 1:      i_ldd(o)
            elif bbb == 2:      i_ph(o,'A')
            elif bbb == 3:      i_jmp(o)
            elif bbb == 4:      i_br(o, VF, 0)  # BVC
            elif bbb == 5:      i_ldd(o)
            elif bbb == 6:      i_cl(o, IF)
            elif bbb == 7:      i_ldd(o)
            else:               u_nop(o)
        elif aaa == 3:
            if bbb == 0:        i_rts(o)
            elif bbb == 1:      i_stz(o)
            elif bbb == 2:      i_pl(o,'A')
            elif bbb == 3:      i_jmpi(o)
            elif bbb == 4:      i_br(o, VF, VF) # BVS
            elif bbb == 5:      i_stz(o)
            elif bbb == 6:      i_se(o, IF)
            elif bbb == 7:      i_jmpix(o) # TODO jmp (abs,x) - check cycle count
            else:               u_nop(o)
        elif aaa == 4:
            if bbb == 0:        i_bra(o)        # BRA
            elif bbb == 2:      i_dey(o)
            elif bbb == 4:      i_br(o, CF, 0)  # BCC
            elif bbb == 6:      i_tya(o)
            elif bbb == 7:      i_stz(o)
            else:               i_sty(o)
        elif aaa == 5:
            if bbb == 2:        i_tay(o)
            elif bbb == 4:      i_br(o, CF, CF) # BCS
            elif bbb == 6:      i_cl(o, VF)
            else:               i_ldy(o)
        elif aaa == 6:
            if bbb == 2:        i_iny(o)
            elif bbb == 4:      i_br(o, ZF, 0)  # BNE
            elif bbb == 5:      i_ldd(o)
            elif bbb == 6:      i_cl(o, DF)
            elif bbb == 7:      i_ldd(o)
            else:               i_cpy(o)
        elif aaa == 7:
            if bbb == 2:        i_inx(o)
            elif bbb == 4:      i_br(o, ZF, ZF) # BEQ
            elif bbb == 5:      i_ldd(o)
            elif bbb == 6:      i_se(o, DF)
            elif bbb == 7:      i_ldd(o)
            else:               i_cpx(o)
    elif cc == 1:
        if aaa == 0:    i_ora(o)
        elif aaa == 1:  i_and(o)
        elif aaa == 2:  i_eor(o)
        elif aaa == 3:  i_adc(o)
        elif aaa == 4:
            if bbb == 2:    i_bit(o)
            else:           i_sta(o)
        elif aaa == 5:  i_lda(o)
        elif aaa == 6:  i_cmp(o)
        else:           i_sbc(o)
    elif cc == 2:
        if aaa == 0:
            if bbb == 0:    i_ldd(o)
            elif bbb == 2:  i_asla(o)
            elif bbb == 4:  i_ora(o)
            elif bbb == 6:  i_inca(o)
            else:           i_asl(o)
        elif aaa == 1:
            if bbb == 0:    i_ldd(o)
            elif bbb == 2:  i_rola(o)
            elif bbb == 4:  i_and(o)
            elif bbb == 6:  i_deca(o)
            else:           i_rol(o)
        elif aaa == 2:
            if bbb == 0:    i_ldd(o)
            elif bbb == 2:  i_lsra(o)
            elif bbb == 4:  i_eor(o)
            elif bbb == 6:  i_ph(o,'Y')
            else:           i_lsr(o)
        elif aaa == 3:
            if bbb == 0:    i_ldd(o)
            elif bbb == 2:  i_rora(o)
            elif bbb == 4:  i_adc(o)
            elif bbb == 6:  i_pl(o,'Y')
            else:           i_ror(o)
        elif aaa == 4:
            if bbb == 0:    i_ldd(o)
            elif bbb == 2:  i_txa(o)
            elif bbb == 4:  i_sta(o)
            elif bbb == 6:  i_txs(o)
            elif bbb == 7:  i_stz(o)
            else:           i_stx(o)
        elif aaa == 5:
            if bbb == 0:    i_ldx(o)
            elif bbb == 2:  i_tax(o)
            elif bbb == 4:  i_lda(o)
            elif bbb == 6:  i_tsx(o)
            else:           i_ldx(o)
        elif aaa == 6:
            if bbb == 0:        i_ldd(o)
            elif bbb == 2:      i_dex(o)
            elif bbb == 4:      i_cmp(o)
            elif bbb == 6:      i_ph(o,'X')
            else:               i_dec(o)
        elif aaa == 7:
            if bbb == 0:        i_ldd(o)
            elif bbb == 2:      i_nop(o)
            elif bbb == 4:      i_sbc(o)
            elif bbb == 6:      i_pl(o,'X')
            else:               i_inc(o)
    elif cc == 3:
        raise Exception("should not be reached")
    # fetch next opcode byte
    if mem_access in [M_R_,M___]:
        o.ta('_FETCH();')
    else:
        o.t('_FETCH();')
    return o

def write_result():
    with open(INOUT_PATH, 'r') as f:
        lines = f.read().splitlines()
        lines = templ.replace(lines, 'decoder', out_lines)
    out_str = '\n'.join(lines) + '\n'
    out_str = out_str.replace("M6502", "M65C02")
    out_str = out_str.replace("m6502", "m65c02")
    # out_str = out_str.replace("M6510", "__hidden_M6510")  # just hide those
    # out_str = out_str.replace("m6510", "__hidden_m6510")  # just hide those
    with open(INOUT_PATH, 'w') as f:
        f.write(out_str)

if __name__ == '__main__':
    for op in range(0, 256):
        write_op(enc_op(op))
    write_result()
