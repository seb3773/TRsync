#include "inifile.h"

#include <ntqfile.h>
#include <ntqtextstream.h>

static TQString trim(const TQString &s)
{
    TQString t = s;
    t = t.stripWhiteSpace();
    return t;
}

IniFile::IniFile()
    : m_groups()
{
}

bool IniFile::load(const TQString &path)
{
    m_groups.clear();

    TQFile f(path);
    if (!f.open(IO_ReadOnly)) return false;

    TQTextStream ts(&f);

    TQString current;
    while (!ts.eof()) {
        TQString line = ts.readLine();
        line = trim(line);
        if (line.isEmpty()) continue;
        if (line[0] == '#') continue;
        if (line[0] == ';') continue;

        if (line[0] == '[') {
            const int end = line.find(']');
            if (end > 1) {
                current = trim(line.mid(1, end - 1));
                ensureGroup(current);
            }
            continue;
        }

        const int eq = line.find('=');
        if (eq <= 0) continue;

        const TQString key = trim(line.left(eq));
        const TQString val = trim(line.mid(eq + 1));

        if (current.isEmpty()) continue;

        m_groups[current][key] = val;
    }

    return true;
}

bool IniFile::save(const TQString &path) const
{
    TQFile f(path);
    if (!f.open(IO_WriteOnly | IO_Truncate)) return false;

    TQTextStream ts(&f);

    for (Groups::ConstIterator git = m_groups.begin(); git != m_groups.end(); ++git) {
        ts << "[" << git.key() << "]\n";
        const KV &kv = git.data();
        for (KV::ConstIterator it = kv.begin(); it != kv.end(); ++it) {
            ts << it.key() << "=" << it.data() << "\n";
        }
        ts << "\n";
    }

    return true;
}

void IniFile::ensureGroup(const TQString &name)
{
    if (!m_groups.contains(name)) m_groups.insert(name, KV());
}

void IniFile::removeGroup(const TQString &name)
{
    m_groups.remove(name);
}

TQStringList IniFile::groups() const
{
    TQStringList out;
    for (Groups::ConstIterator it = m_groups.begin(); it != m_groups.end(); ++it) {
        out << it.key();
    }
    return out;
}

IniGroup IniFile::group(const TQString &name)
{
    ensureGroup(name);
    return IniGroup(this, name);
}

IniGroup::IniGroup()
    : m_file(0)
    , m_name()
{
}

IniGroup::IniGroup(IniFile *file, const TQString &name)
    : m_file(file)
    , m_name(name)
{
}

TQString IniGroup::readString(const TQString &key, const TQString &def) const
{
    if (!m_file) return def;
    if (!m_file->m_groups.contains(m_name)) return def;

    const IniFile::KV &kv = m_file->m_groups[m_name];
    IniFile::KV::ConstIterator it = kv.find(key);
    if (it == kv.end()) return def;

    return it.data();
}

void IniGroup::writeString(const TQString &key, const TQString &value)
{
    if (!m_file) return;
    m_file->ensureGroup(m_name);
    m_file->m_groups[m_name][key] = value;
}

bool IniGroup::readBool(const TQString &key, bool def) const
{
    const TQString val = readString(key, def ? "true" : "false");
    return (val == "true" || val == "1" || val == "yes");
}

void IniGroup::writeBool(const TQString &key, bool value)
{
    writeString(key, value ? "true" : "false");
}

int IniGroup::readInt(const TQString &key, int def) const
{
    const TQString val = readString(key, TQString::number(def));
    bool ok = false;
    const int result = val.toInt(&ok);
    return ok ? result : def;
}

void IniGroup::writeInt(const TQString &key, int value)
{
    writeString(key, TQString::number(value));
}
