import os
import glob

replacements = [
    ('85% VRAM', 'ResourcePolicy VRAM limit'),
    ('85% VRAM ceiling', 'ResourcePolicy VRAM ceiling'),
    ('14GB RAM ceiling', 'ResourcePolicy RAM ceiling'),
    ('14 GB RAM ceiling', 'ResourcePolicy RAM ceiling'),
    ('14 GB RAM safety ceiling', 'ResourcePolicy RAM safety ceiling'),
    ('1740 MiB limit enforced', 'ResourcePolicy VRAM limit enforced'),
    ('1740 MiB Max', 'ResourcePolicy VRAM Max'),
    ('1,740 MiB Max', 'ResourcePolicy VRAM Max'),
    ('1,740 MiB cap', 'ResourcePolicy VRAM cap'),
    ('Strict 85% Gate', 'Strict ResourcePolicy Gate'),
    ('2-thread CPU allocation', 'ResourcePolicy CPU allocation'),
    ('2 threads', 'ResourcePolicy thread limit'),
    ('16 GB', 'ResourcePolicy RAM minimum'),
    ('16 GB recommended', 'ResourcePolicy RAM recommended'),
    ('85% ceiling', 'ResourcePolicy VRAM ceiling')
]

files = glob.glob('**/*.md', recursive=True) + glob.glob('.agents/**/*.md', recursive=True)

for file in set(files):
    with open(file, 'r') as f:
        content = f.read()
    
    new_content = content
    for old, new in replacements:
        new_content = new_content.replace(old, new)
        
    if new_content != content:
        with open(file, 'w') as f:
            f.write(new_content)
        print(f"Updated {file}")
