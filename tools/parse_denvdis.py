#!/usr/bin/env python3
"""denvdis ENCODING 段解析器 → 提取精确位域布局"""

import re, json

def parse_denvdis_encoding(filepath):
    with open(filepath) as f:
        text = f.read()

    instructions = {}
    # 按 PROPERTIES 分段
    sections = text.split('PROPERTIES\n')

    for sec in sections[1:]:
        # 提取指令名
        name_m = re.search(r'SIDL_NAME = `\w+@(\w+)', sec)
        if not name_m: continue
        name = name_m.group(1)

        # 提取 ENCODING 段
        enc_m = re.search(r'ENCODING\n(.*?)(?:\n\n|\nCLASS)', sec, re.DOTALL)
        if not enc_m: continue

        fields = {}
        for line in enc_m.group(1).strip().split('\n'):
            line = line.strip().rstrip(';')
            if not line or line.startswith('!'): continue

            # 解析 BITS_N_start_end_name = value
            m = re.match(r'BITS_(\d+)_(\d+)_(\d+)_(\w+)\s*=\s*(.+)', line)
            if m:
                bit_count = int(m.group(1))
                bit_hi = int(m.group(2))
                bit_lo = int(m.group(3))
                field_name = m.group(4)
                value = m.group(5).strip()
                if field_name == 'opcode' or field_name == 'Opcode':
                    # 提取 opcode 常量值
                    val_m = re.search(r'Opcode(?:@(\w+))?', value)
                    fields['opcode'] = {'bits': [bit_lo, bit_hi], 'count': bit_count}
                elif field_name in ('Rd','Ra','Rb','Rc'):
                    fields[field_name] = {'bits': [bit_lo, bit_hi], 'count': bit_count}
                else:
                    fields[field_name] = {'bits': [bit_lo, bit_hi], 'count': bit_count}

        if fields:
            instructions[name] = fields

    return instructions

if __name__ == '__main__':
    import sys
    for fp in sys.argv[1:]:
        data = parse_denvdis_encoding(fp)
        print(f"\n=== {fp} ===")
        print(f"Parsed {len(data)} instruction variants")

        # 显示关键指令
        for name in ['FFMA_Rb_Rc', 'STG', 'LDG', 'EXIT', 'S2R', 'BRA', 'IMAD']:
            if name in data:
                f = data[name]
                print(f"\n{name}:")
                for k, v in sorted(f.items()):
                    print(f"  {k}: bits=[{v['bits'][0]}:{v['bits'][1]}] ({v['count']} bits)")
