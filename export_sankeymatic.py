import sqlite3
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 export_sankeymatic.py <database.sqlite>")
        sys.exit(1)
        
    db_path = sys.argv[1]
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    
    # Let's get the active project (assume project 1 for testing)
    project_id = 1
    
    # Get tags
    c.execute("SELECT id, name FROM tags WHERE project_id = ?", (project_id,))
    tags = {row[0]: row[1] for row in c.fetchall()}
    
    # Get co-occurrences
    # For Sankey, we usually want directed flow, but co-occurrence is symmetric.
    # We'll just output an edge for each co-occurrence pair where id1 < id2.
    c.execute("""
        SELECT h1.tag_id, h2.tag_id, COUNT(*) as weight
        FROM highlights h1
        JOIN highlights h2 ON h1.document_id = h2.document_id 
          AND h1.id != h2.id
          AND h1.tag_id < h2.tag_id
        WHERE h1.project_id = ? AND h2.project_id = ?
        GROUP BY h1.tag_id, h2.tag_id
        HAVING weight > 0
    """, (project_id, project_id))
    
    edges = c.fetchall()
    
    if not edges:
        print("No hay co-ocurrencias para generar el diagrama.")
        sys.exit(0)
        
    print("=== Pega el siguiente texto en SankeyMATIC (https://sankeymatic.com/build/) ===\n")
    for t1, t2, weight in edges:
        name1 = tags.get(t1, f"Tag {t1}")
        name2 = tags.get(t2, f"Tag {t2}")
        print(f"{name1} [{weight}] {name2}")

if __name__ == '__main__':
    main()
