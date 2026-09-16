import os
import pytest 
import psycopg2

@pytest.fixture(scope="class")
def clean_database():
    # Читаем настройки из ENV. 
    # Если переменных нет (локальный запуск), берем дефолты для WSL (порт 5433!)
    db_config = {
        "dbname": os.getenv("DB_NAME", "notes_db"),
        "user": os.getenv("DB_USER", "defect"),
        "password": os.getenv("DB_PASSWORD", "Qwe741741qwe"),
        "host": os.getenv("DB_HOST", "127.0.0.1"), # В докере будет 'db', локально '127.0.0.1'
        "port": os.getenv("DB_PORT", "5433")        # В докере будет '5432', локально '5433'
    }

    # Подключаемся
    conn = psycopg2.connect(**db_config)
    cur = conn.cursor()
    
    # Твоя логика очистки (она отличная, оставляем как есть)
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