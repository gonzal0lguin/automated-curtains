"""Tiny KiCad schematic generator: symbols + stubs/labels/power symbols + wires."""
import re, uuid, math, copy

STOCK = '/usr/share/kicad/symbols'
NS = uuid.UUID('9f53d7c2-f09d-4ba7-af4b-7502d0827958')
ROOT = str(NS)


class Q(str):
    """quoted string atom"""


def parse(text):
    tok = re.compile(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+')
    stack, cur = [], []
    for m in tok.finditer(text):
        t = m.group(0)
        if t == '(':
            stack.append(cur); cur = []
        elif t == ')':
            done = cur; cur = stack.pop(); cur.append(done)
        elif t[0] == '"':
            cur.append(Q(t[1:-1].replace('\\"', '"').replace('\\\\', '\\')))
        else:
            cur.append(t)
    return cur[0]


def dump(n, ind=0):
    if isinstance(n, Q):
        return '"' + n.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'
    if not isinstance(n, list):
        return str(n)
    if all(not isinstance(c, list) for c in n):
        return '(' + ' '.join(dump(c) for c in n) + ')'
    out = '(' + ' '.join(dump(c) for c in n if not isinstance(c, list))
    for c in n:
        if isinstance(c, list):
            out += '\n' + '\t' * (ind + 1) + dump(c, ind + 1)
    return out + '\n' + '\t' * ind + ')'


def f(v):
    v = round(float(v), 4)
    return str(int(v)) if v == int(v) else repr(v)


def uid(key):
    return Q(str(uuid.uuid5(NS, key)))


def child(n, name):
    for c in n:
        if isinstance(c, list) and c and c[0] == name:
            return c
    return None


def children(n, name):
    return [c for c in n if isinstance(c, list) and c and c[0] == name]


_libcache = {}


def load_lib(path):
    if path not in _libcache:
        tree = parse(open(path).read())
        _libcache[path] = {s[1]: s for s in children(tree, 'symbol')}
    return _libcache[path]


def get_symbol(lib_id, libpaths):
    lib, name = lib_id.split(':')
    syms = load_lib(libpaths.get(lib, f'{STOCK}/{lib}.kicad_sym'))
    s = copy.deepcopy(syms[name])
    ext = child(s, 'extends')
    if ext:
        parent = copy.deepcopy(syms[ext[1]])
        pname = parent[1]
        props = {p[1]: p for p in children(s, 'property')}
        new = []
        for c in parent:
            if isinstance(c, list) and c[0] == 'property':
                new.append(props.pop(c[1], c))
            elif isinstance(c, list) and c[0] == 'symbol':
                c[1] = Q(name + c[1][len(pname):]); new.append(c)
            else:
                new.append(c)
        idx = max(i for i, c in enumerate(new) if isinstance(c, list) and c[0] == 'property') + 1
        new[idx:idx] = list(props.values())
        s = new
    s[1] = Q(lib_id)
    return s


def sym_pins(s):
    """{number: (x, y, angle, name, type)} in library coordinates"""
    pins = {}
    for u in children(s, 'symbol'):
        for p in children(u, 'pin'):
            at = child(p, 'at')
            pins[str(child(p, 'number')[1])] = (float(at[1]), float(at[2]), float(at[3]),
                                                str(child(p, 'name')[1]), p[1])
    return pins


M = {0: (1, 0, 0, -1), 90: (0, -1, -1, 0), 180: (-1, 0, 0, 1), 270: (0, 1, 1, 0)}


def tf(rot, x, y):
    a, b, c, d = M[rot]
    return (a * x + b * y, c * x + d * y)


POWER = {'GND': 'power:GND', '+12V': 'power:+12V', '+5V': 'power:+5V', '+3V3': 'power:+3.3V'}
FONT = ['effects', ['font', ['size', '1.27', '1.27']]]


def eff(just=None, hide=False, size=1.27, bold=False):
    font = ['font', ['size', f(size), f(size)]]
    if bold:
        font.append(['bold', 'yes'])
    e = ['effects', font]
    if just:
        e.append(['justify'] + just.split())
    if hide:
        e.append(['hide', 'yes'])
    return e


class Sch:
    def __init__(self, project, libpaths, paper='A3'):
        self.project, self.libpaths, self.paper = project, libpaths, paper
        self.libsyms = {}
        self.items = []
        self.wires = []          # [(x1,y1,x2,y2)]
        self.points = set()      # pin / label / power anchor points
        self.juncs = set()
        self.intent = {}         # (ref, pin) -> net
        self.pinpos = {}         # 'REF.pin' -> (x, y)
        self.npwr = 0
        self.title = []

    # ---------- library ----------
    def lib(self, lib_id):
        if lib_id not in self.libsyms:
            self.libsyms[lib_id] = get_symbol(lib_id, self.libpaths)
        return self.libsyms[lib_id]

    # ---------- primitives ----------
    def wire(self, *pts):
        pts = [self.pt(p) for p in pts]
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            assert x1 == x2 or y1 == y2, f'diagonal wire {pts}'
            if (x1, y1) != (x2, y2):
                self.wires.append((round(x1, 4), round(y1, 4), round(x2, 4), round(y2, 4)))

    def pt(self, p):
        if isinstance(p, str):
            return self.pinpos[p]
        return (round(p[0], 4), round(p[1], 4))

    def route(self, a, b, mode='hv'):
        """L-shaped wire between two pins/points"""
        (x1, y1), (x2, y2) = self.pt(a), self.pt(b)
        mid = (x2, y1) if mode == 'hv' else (x1, y2)
        self.wire((x1, y1), mid, (x2, y2))

    def label(self, name, x, y, left=False):
        x, y = self.pt((x, y))
        self.points.add((x, y))
        self.items.append(['label', Q(name), ['at', f(x), f(y), '180' if left else '0'],
                           eff('right bottom' if left else 'left bottom'),
                           ['uuid', uid(f'label{name}{x}{y}')]])

    def power(self, net, x, y, rot=0):
        x, y = self.pt((x, y))
        self.points.add((x, y))
        lib_id = POWER[net] if net in POWER else net
        s = self.lib(lib_id)
        self.npwr += 1
        ref = f'#PWR{self.npwr:03d}' if lib_id != 'power:PWR_FLAG' else f'#FLG{self.npwr:03d}'
        val = [p for p in children(s, 'property') if p[1] == 'Value'][0]
        vat = child(val, 'at')
        vx, vy = tf(rot, float(vat[1]), float(vat[2]))
        value = lib_id.split(':')[1]
        it = ['symbol', ['lib_id', Q(lib_id)], ['at', f(x), f(y), str(rot)], ['unit', '1'],
              ['exclude_from_sim', 'no'], ['in_bom', 'yes'], ['on_board', 'yes'], ['dnp', 'no'],
              ['uuid', uid(f'pwr{self.npwr}')],
              ['property', Q('Reference'), Q(ref), ['at', f(x), f(y), '0'], eff(hide=True)],
              ['property', Q('Value'), Q(value), ['at', f(x + vx), f(y + vy), '0'], eff()],
              ['property', Q('Footprint'), Q(''), ['at', f(x), f(y), '0'], eff(hide=True)],
              ['property', Q('Datasheet'), Q(''), ['at', f(x), f(y), '0'], eff(hide=True)],
              ['pin', Q('1'), ['uuid', uid(f'pwr{self.npwr}p')]],
              ['instances', ['project', Q(self.project),
                             ['path', Q('/' + ROOT), ['reference', Q(ref)], ['unit', '1']]]]]
        self.items.append(it)

    def flag(self, x, y):
        self.power('power:PWR_FLAG', x, y)

    def nc(self, x, y):
        self.items.append(['no_connect', ['at', f(x), f(y)], ['uuid', uid(f'nc{x}{y}')]])

    def text(self, s, x, y, size=1.27, bold=False):
        self.items.append(['text', Q(s), ['exclude_from_sim', 'no'], ['at', f(x), f(y), '0'],
                           eff('left top', size=size, bold=bold), ['uuid', uid(f'text{s}{x}{y}')]])

    def box(self, x1, y1, x2, y2, title):
        self.items.append(['rectangle', ['start', f(x1), f(y1)], ['end', f(x2), f(y2)],
                           ['stroke', ['width', '0.2'], ['type', 'dash']], ['fill', ['type', 'none']],
                           ['uuid', uid(f'box{title}')]])
        self.text(title, x1 + 1.5, y1 + 1.5, size=2.2, bold=True)

    # ---------- components ----------
    def add(self, ref, lib_id, value, fp, x, y, rot=0, nets=None, lcsc='', stub=2.54,
            refpos=None, valpos=None, dnp=False, extra=None):
        """nets: {pin: 'NET' | '~NET' (wired by hand) | 'NC' | ('NET', stublen)}"""
        s = self.lib(lib_id)
        pins = sym_pins(s)
        nets = {str(k): v for k, v in (nets or {}).items()}
        assert set(nets) == set(pins), f'{ref}: pins {sorted(set(pins) ^ set(nets))} mismatch'
        props = {p[1]: p for p in children(s, 'property')}

        def ppos(name):
            at = child(props[name], 'at')
            dx, dy = tf(rot, float(at[1]), float(at[2]))
            return x + dx, y + dy

        def pjust(name):
            j = child(child(props[name], 'effects'), 'justify')
            return ' '.join(j[1:]) if j and rot == 0 else None

        rp = refpos or ppos('Reference')
        vp = valpos or ppos('Value')
        fj = 'right' if rot in (90, 180) else 'left'
        rj = fj if refpos else pjust('Reference')
        vj = fj if valpos else pjust('Value')
        fa = '90' if rot in (90, 270) else '0'
        it = ['symbol', ['lib_id', Q(lib_id)], ['at', f(x), f(y), str(rot)], ['unit', '1'],
              ['exclude_from_sim', 'no'], ['in_bom', 'yes'], ['on_board', 'yes'],
              ['dnp', 'yes' if dnp else 'no'], ['uuid', uid(f'sym{ref}')],
              ['property', Q('Reference'), Q(ref), ['at', f(rp[0]), f(rp[1]), fa], eff(rj)],
              ['property', Q('Value'), Q(value), ['at', f(vp[0]), f(vp[1]), fa], eff(vj)],
              ['property', Q('Footprint'), Q(fp), ['at', f(x), f(y), '0'], eff(hide=True)],
              ['property', Q('Datasheet'), Q(str(props['Datasheet'][2]) if 'Datasheet' in props else ''),
               ['at', f(x), f(y), '0'], eff(hide=True)],
              ['property', Q('Description'), Q(str(props['Description'][2]) if 'Description' in props else ''),
               ['at', f(x), f(y), '0'], eff(hide=True)]]
        fields = dict(extra or {})
        if lcsc:
            fields['LCSC'] = lcsc
        for k, v in fields.items():
            it.append(['property', Q(k), Q(v), ['at', f(x), f(y), '0'], eff(hide=True)])
        for num in pins:
            it.append(['pin', Q(num), ['uuid', uid(f'sym{ref}pin{num}')]])
        it.append(['instances', ['project', Q(self.project),
                                 ['path', Q('/' + ROOT), ['reference', Q(ref)], ['unit', '1']]]])
        self.items.append(it)

        for num, (px, py, ang, name, typ) in pins.items():
            dx, dy = tf(rot, px, py)
            X, Y = round(x + dx, 4), round(y + dy, 4)
            self.pinpos[f'{ref}.{num}'] = (X, Y)
            self.points.add((X, Y))
            net = nets[num]
            L = stub
            if isinstance(net, tuple):
                net, L = net
            if net == 'NC':
                if typ != 'no_connect':
                    self.nc(X, Y)
                continue
            self.intent[(ref, num)] = net.lstrip('~')
            if net.startswith('~'):
                continue
            ox, oy = tf(rot, -math.cos(math.radians(ang)), -math.sin(math.radians(ang)))
            ox, oy = round(ox), round(oy)
            ex, ey = round(X + ox * L, 4), round(Y + oy * L, 4)
            if net in POWER:
                up = net != 'GND'
                # vertical pins get the symbol right on the stub; never draw it upside-down
                prot = 0
                if (oy == -1 and not up) or (oy == 1 and up):
                    prot = 180
                if L:
                    self.wire((X, Y), (ex, ey))
                self.power(net, ex, ey, prot)
            else:
                self.wire((X, Y), (ex, ey))
                self.label(net, ex, ey, left=(ox == -1))

    # ---------- output ----------
    def _split(self):
        pts = set(self.points) | self.juncs
        for w in self.wires:
            pts.add((w[0], w[1])); pts.add((w[2], w[3]))
        out = []
        for (x1, y1, x2, y2) in self.wires:
            inner = [p for p in pts if p != (x1, y1) and p != (x2, y2) and
                     min(x1, x2) <= p[0] <= max(x1, x2) and min(y1, y2) <= p[1] <= max(y1, y2)]
            inner.sort(key=lambda p: abs(p[0] - x1) + abs(p[1] - y1))
            chain = [(x1, y1)] + inner + [(x2, y2)]
            out += [(a[0], a[1], b[0], b[1]) for a, b in zip(chain, chain[1:])]
        self.wires = sorted(set(out))
        # junction wherever 3+ wire ends / anchors meet
        cnt = {}
        for (x1, y1, x2, y2) in self.wires:
            for p in ((x1, y1), (x2, y2)):
                cnt[p] = cnt.get(p, 0) + 1
        for p, n in cnt.items():
            if n >= 3 or (n == 2 and p in self.points):
                self.juncs.add(p)

    def render(self):
        self._split()
        top = ['kicad_sch', ['version', '20260306'], ['generator', Q('eeschema')],
               ['generator_version', Q('10.0')], ['uuid', Q(ROOT)], ['paper', Q(self.paper)]]
        if self.title:
            top.append(self.title)
        top.append(['lib_symbols'] + [self.libsyms[k] for k in sorted(self.libsyms)])
        for (x, y) in sorted(self.juncs):
            top.append(['junction', ['at', f(x), f(y)], ['diameter', '0'], ['color', '0', '0', '0', '0'],
                        ['uuid', uid(f'j{x}{y}')]])
        for w in self.wires:
            top.append(['wire', ['pts', ['xy', f(w[0]), f(w[1])], ['xy', f(w[2]), f(w[3])]],
                        ['stroke', ['width', '0'], ['type', 'default']], ['uuid', uid(f'w{w}')]])
        top += self.items
        top.append(['sheet_instances', ['path', Q('/'), ['page', Q('1')]]])
        top.append(['embedded_fonts', 'no'])
        return dump(top) + '\n'


def check_netlist(path, intent):
    """compare a kicad-cli kicadsexpr netlist with the intended pin->net map"""
    tree = parse(open(path).read())
    got = {}
    for net in children(child(tree, 'nets'), 'net'):
        name = str(child(net, 'name')[1]).lstrip('/')
        for node in children(net, 'node'):
            got[(str(child(node, 'ref')[1]), str(child(node, 'pin')[1]))] = name
    alias = {'+3.3V': '+3V3'}
    bad = 0
    anon = {}
    for k, want in sorted(intent.items()):
        g = alias.get(got.get(k), got.get(k))
        if want.startswith('$'):
            anon.setdefault(want, set()).add(g)
            if g is None or not g.startswith('Net-('):
                bad += 1; print('MISMATCH', k, 'want anonymous', want, 'got', g)
            continue
        if g != want:
            bad += 1; print('MISMATCH', k, 'want', want, 'got', g)
    for want, names in anon.items():
        if len(names) != 1:
            bad += 1; print('SPLIT NET', want, names)
    seen = {}
    for want, names in anon.items():
        for n in names:
            if n in seen:
                bad += 1; print('MERGED NETS', want, seen[n])
            seen[n] = want
    for k, g in sorted(got.items()):
        if k not in intent and not g.startswith('unconnected'):
            bad += 1; print('UNEXPECTED', k, g)
    nets = {}
    for k, v in got.items():
        nets.setdefault(v, []).append(k)
    for n, v in sorted(nets.items()):
        if len(v) == 1 and not n.startswith('unconnected'):
            print('SINGLE-PIN NET', n, v)
    print(f'netlist check: {len(intent)} pins intended, {bad} problems')
    return bad
