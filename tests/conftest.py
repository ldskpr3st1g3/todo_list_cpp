import pytest 
import psycopg2

@pytest.fixture(scope="class")
def clean_database():
    conn = psycopg2.connect(
        dbname="notes_db",
        user="defect",
        password="Qwe741741qwe",
        host="127.0.0.1",
        port="5432"
        )
    cur = conn.cursor();
    cur.execute("""
        DO $$ 
        DECLARE 
            r RECORD;
        BEGIN
            FOR r IN (SELECT tablename FROM pg_tables WHERE schemaname = 'public') 
            LOOP
                EXECUTE 'TRUNCATE TABLE ' || quote_ident(r.tablename) || ' RESTART IDENTITY CASCADE';
            END LOOP;
        END $$;
    """)

    conn.commit()
    cur.close()
    conn.close()
    
    yield