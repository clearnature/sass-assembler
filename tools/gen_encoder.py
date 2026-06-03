#!/usr/bin/env python3
"""denvdis 全指令解析 → 生成 C++ 位精确编码器"""
import re, sys

def parse_all(filepath):
    with open(filepath) as f:
        text = f.read()

    variants = {}
    sections = text.split('PROPERTIES\n')

    for sec in sections[1:]:
        name_m = re.search(r'SIDL_NAME = `\w+@(\w+)', sec)
        if not name_m: continue
        name = name_m.group(1)

        enc_m = re.search(r'ENCODING\n(.*?)(?:\n\n|\nCLASS|\nILLEGAL)', sec, re.DOTALL)
        if not enc_m: continue

        fields = {}
        opcode_bits = []
        for line in enc_m.group(1).strip().split('\n'):
            line = line.strip().rstrip(';')
            if not line or line.startswith('!') or 'TABLES' in line: continue

            m = re.match(r'BITS_(\d+)_(\d+)_(\d+)_(\d+)_(\d+)_(\w+)\s*=\s*(.+)', line)
            if not m:
                m = re.match(r'BITS_(\d+)_(\d+)_(\d+)_(\w+)\s*=\s*(.+)', line)
                if not m: continue
                count, hi, lo, fname, val = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
                fields[fname] = {'bits': f'{hi}:{lo}', 'count': count}
                if fname in ('opcode','Opcode'):
                    opcode_bits.append(f'{hi}:{lo}')
            else:
                count, hi1, lo1, hi2, lo2, fname, val = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6), m.group(7)
                fields[fname] = {'bits': f'{hi1}:{lo1}+{hi2}:{lo2}', 'count': count}
                if fname in ('opcode','Opcode'):
                    opcode_bits.append(f'{hi1}:{lo1}+{hi2}:{lo2}')

        if opcode_bits:
            variants[name] = {
                'opcode_bits': opcode_bits,
                'Rd': fields.get('Rd',{}).get('bits',''),
                'Ra': fields.get('Ra',{}).get('bits',''),
                'Rb': fields.get('Rb',{}).get('bits',''),
                'Rc': fields.get('Rc',{}).get('bits',''),
                'Pg': fields.get('Pg',{}).get('bits',''),
                'fields': {k:v['bits'] for k,v in fields.items() if k not in ('opcode','Opcode','Rd','Ra','Rb','Rc','Pg')}
            }

    return variants

if __name__ == '__main__':
    variants = parse_all(sys.argv[1])
    print(f'// Auto-generated from denvdis: {len(variants)} instruction variants')
    print(f'// Architecture: {sys.argv[1]}')

    # 统计关键族
    families = {}
    for name in variants:
        prefix = re.split(r'[^A-Za-z]', name)[0]
        families.setdefault(prefix, []).append(name)

    print(f'\n// Families: {len(families)}')
    for fam, vars in sorted(families.items()):
        print(f'//   {fam}: {len(vars)} variants')

    # 输出 FFMA 族详细信息
    if 'FFMA' in families:
        print('\n// === FFMA family ===')
        for v in families['FFMA'][:5]:
            info = variants[v]
            print(f'// {v}: opcode_bits={info["opcode_bits"]} Rd={info["Rd"]} Ra={info["Ra"]}')
