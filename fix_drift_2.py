import os
import glob

replacements = [
    ('ResourcePolicy VRAM limit ceiling', 'ResourcePolicy VRAM ceiling'),
    ('ResourcePolicy VRAM limit enforced', 'ResourcePolicy VRAM ceiling enforced'),
    ('ResourcePolicy VRAM limit Ceiling', 'ResourcePolicy VRAM ceiling'),
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
