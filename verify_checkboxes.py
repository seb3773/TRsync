#!/usr/bin/env python3
"""
Script pour vérifier que TOUTES les checkboxes GTK ont leur équivalent TQt3
et que leur état par défaut (active) est identique
"""

import re
import sys

# Liste complète des checkboxes extraites du glade avec leur état par défaut
GTK_CHECKBOXES = {
    # Onglet Basic
    'check_time': {'label': 'Preserve time', 'active': True},
    'check_perm': {'label': 'Preserve permissions', 'active': False},
    'check_owner': {'label': 'Preserve owner', 'active': False},
    'check_group': {'label': 'Preserve group', 'active': False},
    'check_delete': {'label': 'Delete on destination', 'active': False},
    'check_onefs': {'label': 'Do not leave filesystem', 'active': False},
    'check_verbose': {'label': 'Verbose', 'active': True},
    'check_progr': {'label': 'Show transfer progress', 'active': True},
    
    # Onglet Advanced
    'check_exist': {'label': 'Ignore existing', 'active': False},
    'check_size': {'label': 'Size only', 'active': False},
    'check_skipnew': {'label': 'Skip newer', 'active': False},
    'check_windows': {'label': 'Windows compatibility', 'active': False},
    'check_sum': {'label': 'Always checksum', 'active': False},
    'check_compr': {'label': 'Compress file data', 'active': False},
    'check_dev': {'label': 'Preserve devices', 'active': False},
    'check_update': {'label': 'Only update existing files', 'active': False},
    'check_keepart': {'label': 'Keep partially transferred files', 'active': False},
    'check_mapuser': {'label': "Don't map uid/gid values", 'active': False},
    'check_symlink': {'label': 'Copy symlinks as symlinks', 'active': False},
    'check_hardlink': {'label': 'Copy hardlinks as hardlinks', 'active': False},
    'check_backup': {'label': 'Make backups', 'active': False},
    'check_itemized': {'label': 'Show itemized changes list', 'active': False},
    'check_norecur': {'label': 'Disable recursion', 'active': False},
    'check_protectargs': {'label': 'Protect remote args', 'active': True},
    
    # Onglet Extra (commands)
    'check_com_before': {'label': 'Execute this command before rsync:', 'active': False},
    'check_com_halt': {'label': 'Halt on failure', 'active': False},
    'check_com_after': {'label': 'Execute this command after rsync:', 'active': False},
    'check_com_onerror': {'label': 'On rsync error only', 'active': False},
    
    # Onglet Additional (preferences)
    'check_browse_files': {'label': 'Browse files instead of folders', 'active': False},
    'check_superuser': {'label': 'Run as superuser', 'active': False},
    'check_output': {'label': 'Show rsync output by default', 'active': False},
    'check_remember': {'label': 'Remember last used session', 'active': False},
    'check_errorlist': {'label': 'Show error list when finished', 'active': False},
    'check_log': {'label': 'Enable logging', 'active': False},
    'check_fastscroll': {'label': 'Fast rsync output scrolling', 'active': False},
    'check_switchbutton': {'label': 'Enable switch button', 'active': False},
    'check_trayicon': {'label': 'Use tray icon', 'active': False},
    'check_log_overwrite': {'label': 'Overwrite logs', 'active': False},
    'checkbutton_isset': {'label': 'Add as session set', 'active': False},
}

def check_tqt3_implementation(mainwindow_cpp):
    """Vérifie que chaque checkbox GTK a son équivalent TQt3"""
    
    with open(mainwindow_cpp, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print("=" * 80)
    print("VÉRIFICATION DES CHECKBOXES GTK → TQt3")
    print("=" * 80)
    print()
    
    missing = []
    found = []
    wrong_default = []
    
    for gtk_id, info in GTK_CHECKBOXES.items():
        label = info['label']
        expected_active = info['active']
        
        # Chercher le TQCheckBox correspondant dans le code
        # Pattern: m_xxx = new TQCheckBox("label", ...)
        # ou: m_xxx->setText("label")
        
        # Essayer de trouver par le label
        pattern = rf'new TQCheckBox\s*\(\s*["\']({re.escape(label)})["\']'
        match = re.search(pattern, content, re.IGNORECASE)
        
        if match:
            # Trouvé ! Maintenant vérifier l'état par défaut
            # Chercher setChecked après la création
            var_pattern = rf'(m_\w+)\s*=\s*new TQCheckBox\s*\(["\']({re.escape(label)})["\']'
            var_match = re.search(var_pattern, content, re.IGNORECASE)
            
            if var_match:
                var_name = var_match.group(1)
                # Chercher setChecked(true/false) pour cette variable
                checked_pattern = rf'{re.escape(var_name)}\s*->\s*setChecked\s*\(\s*(true|false)\s*\)'
                checked_match = re.search(checked_pattern, content)
                
                if checked_match:
                    actual_active = (checked_match.group(1) == 'true')
                    if actual_active != expected_active:
                        wrong_default.append({
                            'gtk_id': gtk_id,
                            'label': label,
                            'var': var_name,
                            'expected': expected_active,
                            'actual': actual_active
                        })
                    else:
                        found.append({'gtk_id': gtk_id, 'label': label, 'var': var_name})
                else:
                    # Pas de setChecked trouvé, défaut = false
                    if expected_active:
                        wrong_default.append({
                            'gtk_id': gtk_id,
                            'label': label,
                            'var': var_name,
                            'expected': expected_active,
                            'actual': False
                        })
                    else:
                        found.append({'gtk_id': gtk_id, 'label': label, 'var': var_name})
        else:
            missing.append({'gtk_id': gtk_id, 'label': label})
    
    # Rapport
    print(f"✓ TROUVÉES ET CORRECTES: {len(found)}/{len(GTK_CHECKBOXES)}")
    print(f"✗ MANQUANTES: {len(missing)}")
    print(f"⚠ ÉTAT PAR DÉFAUT INCORRECT: {len(wrong_default)}")
    print()
    
    if missing:
        print("=" * 80)
        print("CHECKBOXES MANQUANTES")
        print("=" * 80)
        for item in missing:
            print(f"  ✗ {item['gtk_id']}: \"{item['label']}\"")
        print()
    
    if wrong_default:
        print("=" * 80)
        print("CHECKBOXES AVEC ÉTAT PAR DÉFAUT INCORRECT")
        print("=" * 80)
        for item in wrong_default:
            print(f"  ⚠ {item['gtk_id']} ({item['var']}): \"{item['label']}\"")
            print(f"     Attendu: {item['expected']}, Actuel: {item['actual']}")
        print()
    
    if found:
        print("=" * 80)
        print(f"CHECKBOXES CORRECTES ({len(found)})")
        print("=" * 80)
        for item in found[:10]:  # Afficher seulement les 10 premières
            print(f"  ✓ {item['gtk_id']} → {item['var']}: \"{item['label']}\"")
        if len(found) > 10:
            print(f"  ... et {len(found) - 10} autres")
        print()
    
    # Résumé final
    print("=" * 80)
    print("RÉSUMÉ")
    print("=" * 80)
    total = len(GTK_CHECKBOXES)
    ok = len(found)
    percentage = (ok / total) * 100 if total > 0 else 0
    
    print(f"Conformité: {ok}/{total} ({percentage:.1f}%)")
    
    if missing or wrong_default:
        print("\n⚠ LE PORTAGE N'EST PAS COMPLET !")
        print(f"   - {len(missing)} checkboxes manquantes")
        print(f"   - {len(wrong_default)} états par défaut incorrects")
        return False
    else:
        print("\n✓ TOUTES LES CHECKBOXES SONT CORRECTEMENT PORTÉES !")
        return True

if __name__ == '__main__':
    mainwindow_cpp = 'src/mainwindow.cpp'
    success = check_tqt3_implementation(mainwindow_cpp)
    sys.exit(0 if success else 1)

# Made with Bob
