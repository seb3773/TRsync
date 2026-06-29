#ifndef GRSYNC_TQT3_INIFILE_H
#define GRSYNC_TQT3_INIFILE_H

#include <ntqmap.h>
#include <ntqstring.h>
#include <ntqstringlist.h>

class IniGroup;

class IniFile
{
public:
    IniFile();

    bool load(const TQString &path);
    bool save(const TQString &path) const;

    void ensureGroup(const TQString &name);
    void removeGroup(const TQString &name);
    TQStringList groups() const;

    IniGroup group(const TQString &name);

private:
    friend class IniGroup;

    typedef TQMap<TQString, TQString> KV;
    typedef TQMap<TQString, KV> Groups;

    Groups m_groups;
};

class IniGroup
{
public:
    IniGroup();
    IniGroup(IniFile *file, const TQString &name);

    TQString readString(const TQString &key, const TQString &def) const;
    void writeString(const TQString &key, const TQString &value);
    
    bool readBool(const TQString &key, bool def) const;
    void writeBool(const TQString &key, bool value);
    
    int readInt(const TQString &key, int def) const;
    void writeInt(const TQString &key, int value);

private:
    IniFile *m_file;
    TQString m_name;
};

#endif
