import numpy as np
import pandas as pd
import hashlib
import re
from pathlib import Path

data = []
for path in Path('./handin').glob('*.sv'):
    sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
    data.append({'stu': path.stem, 'sha256': sha256[:8]})
stu_df = pd.DataFrame(data)

data = []
for path in Path('spmm/eval/l1').glob('*.txt'):
    name = path.stem
    for tb, score in re.findall(r'../../../(.+).tb.cpp L1 SCORE: (\d+)', path.read_text()):
        data.append({
            'stu': name,
            'type': 'level-1',
            'tb': tb,
            'score': float(score) * 20
        })
pat = re.compile(r'(COMPONENT|COMPLEXITY|FINAL) SCORE\s*:\s*([\d.]+)  /')
for path in Path('spmm/eval/l2').glob('*.txt'):
    name = path.stem
    for tb, score in pat.findall(path.read_text()):
        if tb == 'FINAL': continue
        data.append({
            'stu': name,
            'type': 'level-2',
            'tb': tb.lower(),
            'score': float(score) * 2.5
        })
df = pd.DataFrame(data)

score_df = df.pivot_table(values='score', index='stu', columns=['type', 'tb'])
score_df.insert(0, 'sha256', stu_df.set_index('stu').sha256)
score_df.insert(1, 'rdu', np.nan)
score_df['final'] = df.groupby('stu').score.sum()

s = score_df.sort_values('final', ascending=False).style
s = s.background_gradient(vmin=0, vmax=20, axis=0, subset=['level-1'])
s = s.background_gradient(vmin=0, vmax=10, axis=0, subset=['level-2'])
s = s.background_gradient(vmin=60, vmax=80, axis=0, subset=['final'])
s = s.format('{:.2f}', subset=['level-1', 'level-2', 'final'])
s.to_excel('result.xlsx')