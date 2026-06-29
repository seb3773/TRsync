#!/usr/bin/env python3
"""
Script pour extraire TOUS les widgets du fichier grsync.glade
et générer un rapport de comparaison avec l'implémentation TQt3
"""

import xml.etree.ElementTree as ET
import sys

def extract_widgets(glade_file):
    """Extrait tous les widgets du fichier glade"""
    tree = ET.parse(glade_file)
    root = tree.getroot()
    
    widgets = []
    
    def process_element(elem, parent_id="", depth=0):
        """Traite récursivement chaque élément"""
        if elem.tag == 'object':
            widget_class = elem.get('class', '')
            widget_id = elem.get('id', '')
            
            # Extraire les propriétés importantes
            properties = {}
            signals = []
            
            for child in elem:
                if child.tag == 'property':
                    prop_name = child.get('name', '')
                    prop_value = child.text or ''
                    properties[prop_name] = prop_value
                elif child.tag == 'signal':
                    sig_name = child.get('name', '')
                    sig_handler = child.get('handler', '')
                    signals.append((sig_name, sig_handler))
            
            widget_info = {
                'class': widget_class,
                'id': widget_id,
                'parent': parent_id,
                'depth': depth,
                'properties': properties,
                'signals': signals
            }
            
            widgets.append(widget_info)
            
            # Traiter les enfants
            for child in elem:
                process_element(child, widget_id, depth + 1)
        else:
            # Traiter les enfants même si ce n'est pas un object
            for child in elem:
                process_element(child, parent_id, depth)
    
    process_element(root)
    return widgets

def generate_report(widgets, output_file):
    """Génère un rapport markdown des widgets"""
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write("# RAPPORT COMPLET DES WIDGETS GRSYNC\n\n")
        f.write("## Extraction automatique du fichier grsync.glade\n\n")
        
        # Grouper par type de widget
        by_class = {}
        for w in widgets:
            cls = w['class']
            if cls not in by_class:
                by_class[cls] = []
            by_class[cls].append(w)
        
        f.write(f"## Statistiques\n\n")
        f.write(f"- **Total widgets**: {len(widgets)}\n")
        f.write(f"- **Types différents**: {len(by_class)}\n\n")
        
        f.write("### Répartition par type\n\n")
        for cls in sorted(by_class.keys()):
            f.write(f"- **{cls}**: {len(by_class[cls])} widgets\n")
        
        f.write("\n---\n\n")
        
        # Détail de chaque widget important
        f.write("## WIDGETS PRINCIPAUX\n\n")
        
        important_widgets = [
            'GtkWindow', 'GtkDialog', 'GtkNotebook', 'GtkCheckButton',
            'GtkEntry', 'GtkButton', 'GtkComboBox', 'GtkSpinButton',
            'GtkTextView', 'GtkProgressBar', 'GtkExpander', 'GtkLabel'
        ]
        
        for widget_class in important_widgets:
            if widget_class in by_class:
                f.write(f"### {widget_class} ({len(by_class[widget_class])} instances)\n\n")
                
                for w in by_class[widget_class]:
                    if w['id']:
                        f.write(f"#### `{w['id']}`\n\n")
                        
                        # Propriétés importantes
                        important_props = ['label', 'tooltip-text', 'active', 'visible', 
                                         'sensitive', 'max-length', 'title']
                        
                        props_to_show = {k: v for k, v in w['properties'].items() 
                                       if k in important_props and v}
                        
                        if props_to_show:
                            f.write("**Propriétés:**\n")
                            for prop, val in props_to_show.items():
                                f.write(f"- `{prop}`: {val}\n")
                            f.write("\n")
                        
                        # Signaux
                        if w['signals']:
                            f.write("**Signaux:**\n")
                            for sig_name, sig_handler in w['signals']:
                                f.write(f"- `{sig_name}` → `{sig_handler}()`\n")
                            f.write("\n")
                        
                        f.write("**Statut TQt3:** ❓ À vérifier\n\n")
                        f.write("---\n\n")
        
        # Section spéciale pour les checkboxes
        f.write("\n## CHECKBOXES (Options cochables)\n\n")
        checkboxes = [w for w in widgets if w['class'] == 'GtkCheckButton']
        
        f.write(f"Total: {len(checkboxes)} checkboxes\n\n")
        
        for cb in checkboxes:
            if cb['id']:
                label = cb['properties'].get('label', 'NO LABEL')
                tooltip = cb['properties'].get('tooltip-text', '')
                active = cb['properties'].get('active', 'False')
                
                f.write(f"### `{cb['id']}`\n")
                f.write(f"- **Label**: {label}\n")
                if tooltip:
                    f.write(f"- **Tooltip**: {tooltip}\n")
                f.write(f"- **Active par défaut**: {active}\n")
                
                if cb['signals']:
                    f.write(f"- **Signaux**: ")
                    for sig_name, sig_handler in cb['signals']:
                        f.write(f"`{sig_name}` → `{sig_handler}()` ")
                    f.write("\n")
                
                f.write(f"- **Statut TQt3**: ❓ À vérifier\n\n")

if __name__ == '__main__':
    glade_file = '../grsync.glade'
    output_file = 'WIDGETS_GLADE_REPORT.md'
    
    print(f"Extraction des widgets de {glade_file}...")
    widgets = extract_widgets(glade_file)
    
    print(f"Génération du rapport dans {output_file}...")
    generate_report(widgets, output_file)
    
    print(f"✓ Rapport généré: {len(widgets)} widgets extraits")

# Made with Bob
