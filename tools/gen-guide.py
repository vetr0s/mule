# Build notes/str-and-strview.html from notes/str-and-strview.tmpl.html, the
# standard-html house style and notes/str-examples/. Run from anywhere.
import html, re, pathlib
repo = pathlib.Path(__file__).resolve().parent.parent
ex = repo / 'notes/str-examples'
style = (pathlib.Path.home() / '.claude/skills/standard-html/assets/style.css').read_text().rstrip()
t = (repo / 'notes/str-and-strview.tmpl.html').read_text()

text = 'Crime and Punishment'
pre = 'Str text     data ──► '
col = len(pre)
cells = ' '.join(text) + ' \\0 ' + ' '.join('·' * 10)
def at(n, s): return ' ' * n + s
p = col + 2 * text.index('P')
spare = col + 2 * len(text) + 3 + 2 * 9
lines = [
    pre + cells,
    '             len 20, capacity 30'.ljust(spare - len('spare capacity ─') ) + 'spare capacity ─┘',
    at(col, '▲') + at(p - col - 1, '▲'),
    'StrView word'.ljust(col - 5) + 'data ┘ len 5'.ljust(p - col + 5) + '│   word = "Crime"',
    'StrView tail'.ljust(p - 5) + 'data ┘ len 10   tail = "Punishment"',
]
diagram = '<pre><code>' + html.escape('\n'.join(lines), quote=False) + '</code></pre>'
t = re.sub(r'<pre><code>Str text.*?</code></pre>', lambda m: '{{diagram}}', t, flags=re.S)

t = t.replace('{{style}}', style).replace('{{diagram}}', diagram)
t = re.sub(r'\{\{code:(\S+?)\}\}', lambda m: '<p class="muted"><span class="path">notes/str-examples/' + m[1] + '</span></p>\n<pre><code>' + html.escape((ex / m[1]).read_text().rstrip(), quote=False) + '</code></pre>', t)
assert '{{' not in t
(repo / 'notes/str-and-strview.html').write_text('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n' + t.replace('<main>', '</head>\n<body>\n<main>', 1) + '</body>\n</html>\n')
