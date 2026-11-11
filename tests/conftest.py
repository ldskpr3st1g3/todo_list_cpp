import os
import pytest 
import psycopg2
import json

@pytest.fixture(scope="session")
def app_config():
    # Загружаем test_config.json из папки tests
    config_path = os.path.join(os.path.dirname(__file__), "test_config.json")
    with open(config_path, "r") as f:
        return json.load(f)

@pytest.fixture(scope="class")
def clean_database(app_config):
    db_config = {
        "dbname": app_config["database"]["dbname"],
        "user": app_config["database"]["user"],
        "password": app_config["database"]["password"],
        "host": app_config["database"]["host"],
        "port": int(app_config["database"]["port"])
    }

    try:
        conn = psycopg2.connect(**db_config)
        cur = conn.cursor()
        
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
    except psycopg2.OperationalError as e:
        pytest.fail(f"Не удалось подключиться к БД для очистки. Проверь DB_HOST и порт. Ошибка: {e}")
    
    yield

class TestGeneral:
    user_id = None
    user_login = None
    user_email = None
    user_password = None
    user2_id = None
    user2_login = None
    user2_email = None
    user2_password = None
    
    @pytest.fixture(autouse=True)
    def setup(self, app_config):
        TestGeneral.user_login = app_config["tests"]["user_login"]
        TestGeneral.user_email = app_config["tests"]["user_email"]
        TestGeneral.user_password = app_config["tests"]["user_password"]
        
        TestGeneral.user2_login = app_config["tests"].get("user2_login", "test_user2")
        TestGeneral.user2_email = app_config["tests"].get("user2_email", "test2@example.com")
        TestGeneral.user2_password = app_config["tests"].get("user2_password", "test_password2")
        
        self.base_url = f"{app_config['server']['host']}:{app_config['server']['port']}/api/v1"