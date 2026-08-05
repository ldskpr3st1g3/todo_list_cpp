import pytest
import requests

BASE_URL = "http://127.0.0.1:5555"


class TestGeneral:
    
    def test_00_register_success(self, clean_database):
        payload = {
            "login" : "test_user",
            "email" : "test@example.com",
            "password" : "test_password"
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/register", json=payload)
        assert response.status_code == 201
        result = response.json()
        
        assert "data" in result
        assert "status" in result and result["status"] == "Success"
        assert "message" in result and result["message"] == "User created"
        user_data = result["data"]
        assert "id" in user_data
        assert "login" in user_data and user_data["login"] == "test_user"
        assert "email" in user_data and user_data["email"] == "test@example.com"
        assert "password_hash" in user_data and user_data["password_hash"].startswith("$argon2id$")
        assert "password" not in user_data
        TestGeneral.user_id = user_data["id"]
        TestGeneral.user_login = user_data["login"]
        TestGeneral.user_email = user_data["email"]
        TestGeneral.user_password = "test_password"


        payload = {
            "login" : "test_user2",
            "email" : "test2@example.com",
            "password" : "test_password2"
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/register", json=payload)
        assert response.status_code == 201
        result = response.json()
        user_data = result["data"]
        TestGeneral.user2_email = user_data["email"]
        TestGeneral.user2_password = "test_password2"
        TestGeneral.user2_login = user_data["login"]
        

    def test_01_register_error_empty_request(self): 
        payload = {}
        response = requests.post(f"{BASE_URL}/api/v1/users/register", json = payload)
        assert response.status_code == 400
        result = response.json()
        assert "errors" in result
        assert "message" in result and result["message"] == "Invalid json"
        assert "status" in result and result["status"] == "Error"


    def test_02_register_error_duplicate_login(self):
        payload =  {
            "login" : "test_user",
            "email" : "test1@example.com",
            "password" : "test_password"
        }

        response = requests.post(f"{BASE_URL}/api/v1/users/register", json = payload)
        assert response.status_code == 409
        result = response.json()
        assert "status" in result and result["status"] == "Error"
        assert "message" in result and result["message"] == "User with this email already exists"


    def test_03_register_error_duplicate_email(self):
        payload = {
            "login" : "test1_user",
            "email" : "test@example.com",
            "password" : "test_password"
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/register", json = payload)
        assert response.status_code == 409
        result = response.json()
        assert "message" in result and result["message"] == "User with this email already exists"
        assert "status" in result and result["status"] == "Error"


    def test_04_login_success(self):
        payload = {
            "email" : TestGeneral.user_email,
            "password" : TestGeneral.user_password
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/login", json=payload)
        assert response.status_code == 201
        result = response.json()
        assert "message" in result and result["message"] == "Login successful"
        assert "status" in result and result["status"] == "Success"
        user_data = result["data"]
        assert "token_type" in user_data and user_data["token_type"] == "Bearer"
        assert "access_token" in user_data
        assert "refresh_token" in user_data

        TestGeneral.user_access_token = user_data["access_token"]
        TestGeneral.user_refresh_token = user_data["refresh_token"]


        payload = {
            "email" : TestGeneral.user2_email,
            "password" : TestGeneral.user2_password
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/login", json=payload)
        assert response.status_code == 201
        result = response.json()
        user_data = result["data"]
        TestGeneral.user2_access_token = user_data["access_token"]
        


    def test_05_login_error_wrong_password(self):
        payload = {
            "email" : TestGeneral.user_email,
            "password" : TestGeneral.user_password + "1"
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/login", json=payload) 
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Access denied"
        assert "status" in result and result["status"] == "Error"

    def test_06_login_error_wrong_email(self):
        payload = {
            "email" : "abc",
            "password" : TestGeneral.user_password
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/login", json = payload)
        assert response.status_code == 404
        result = response.json()
        assert "message" in result and result["message"] == "User not found"
        assert "status" in result and result["status"] == "Error"

    
    def test_07_me(self):
        response = requests.get(f"{BASE_URL}/api/v1/users/me", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "data" in result 
        assert "message" in result and result["message"] == "Current user"
        assert "status" in result and result["status"] == "Success"


    def test_08_me_error_malformed(self):
        response = requests.get(f"{BASE_URL}/api/v1/users/me", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}1"})
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Malformed jwt"
        assert "status" in result and result["status"] == "Error"


    def test_09_refresh(self):
        payload = {
            "refresh_token" : TestGeneral.user_refresh_token
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/refresh",json=payload)
        assert response.status_code == 201
        result = response.json()
        assert "message" in result and result["message"] == "New tokens created"
        assert "status" in result and result["status"] == "Success"
        assert "data" in result
        user_data = result["data"]
        assert "access_token" in user_data
        assert "refresh_token" in user_data
        assert "token_type" in user_data
        TestGeneral.user_access_token = user_data["access_token"]
        TestGeneral.user_refresh_token = user_data["refresh_token"]


    def test_10_refresh_error_wrong_token(self):
        payload = {
            "refresh_token" : TestGeneral.user_refresh_token + "1"
        }
        response = requests.post(f"{BASE_URL}/api/v1/users/refresh", json=payload)
        assert response.status_code == 404
        result = response.json()
        assert "status" in result and result["status"] == "Error"
        assert "message" in result and result["message"] == "Token not found"


    def test_11_logout_success(self):

        response = requests.patch(f"{BASE_URL}/api/v1/users/me/logout", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "message" in result and result["message"] == "Success log out"
        assert "status" in result and result["status"] == "Success"


    def test_12_logout_error_malformed_token(self):
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/logout", headers={"Authorization": f"Bearer {TestGeneral.user_access_token}1"})
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Malformed jwt"
        assert "status" in result and result["status"] == "Error"


    def test_13_refresh_error_revoked_token(self):
        payload = {
            "refresh_token" : TestGeneral.user_refresh_token
        }

        response = requests.post(f"{BASE_URL}/api/v1/users/refresh", json = payload)
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Access denied"
        assert "status" in result and result["status"] == "Error"


    def test_14_create_new_task_success(self):
        payload = {
            "title" : "test_title",
            "content" : "test_content"
        }

        response = requests.post(f"{BASE_URL}/api/v1/notes", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 201
        result = response.json()
        assert "data" in result 
        assert "message" in result and result["message"] == "Note created"
        assert "status" in result and result["status"] == "Success"
        note = result["data"]
        assert "content" in note and note["content"] == "test_content"
        assert "title" in note and note["title"] == "test_title"
        assert "id" in note 
        TestGeneral.note_id = note["id"]


    def test_15_create_new_task_error_empty_title(self):
        payload = {
            "title" : "",
            "content" : "test_content"
        }
        response = requests.post(f"{BASE_URL}/api/v1/notes", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 400
        result = response.json()
        assert "message" in result and result["message"] == "Invalid json"
        assert "status" in result and result["status"] == "Error"
        assert "errors" in result
        errors = result["errors"]
        assert "Field 'title' can't be empty" in errors
        


    def test_16_create_new_task_error_without_title(self):
        payload = {
            "content" : "test_content"
        }

        response = requests.post(f"{BASE_URL}/api/v1/notes", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 400
        result = response.json()

        assert "message" in result and result["message"] == "Invalid json"
        assert "status" in result and result["status"] == "Error"
        errors = result["errors"]
        assert "Missing required field: 'title'" in errors
         


    def test_17_get_tasks_success(self):
        response = requests.get(f"{BASE_URL}/api/v1/notes", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "message" in result and result["message"] == "Notes list"
        assert "status" in result and result["status"] == "Success"
        assert "data" in result


    def test_18_get_note_by_id_success(self):
        response = requests.get(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "message" in result and result["message"] == "Note details"
        assert "status" in result and result["status"] == "Success"
        assert "data" in result


    def test_19_get_note_by_id_error_someone_elses_token(self):
        response=requests.get(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", headers={"Authorization" : f"Bearer {TestGeneral.user2_access_token}"})
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Access denied"


    def test_20_patch_note_by_id_success(self):
        payload= {
            "title" : "new_title",
            "content" : "new_content"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "status" in result and result["status"] == "Success"

    def test_21_patch_note_by_id_error_someone_elses_token(self):
        payload = {
            "title" : "new_title",
            "content" : "new_content"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user2_access_token}"})
        assert response.status_code == 403
        result = response.json()
        assert "message" in result and result["message"] == "Access denied"


    def test_22_patch_note_by_id_error_emtpy_title(self):
        payload = {
            "title" : "",
            "content" : "new_content"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 400
        result = response.json()
        assert "status" in result and result["status"] == "Error"
        assert "message" in result and result["message"] == "Invalid json"
        errors = result["errors"]
        assert "Field 'title' can't be empty" in errors


    def test_23_patch_note_by_id_success_without_title(self):
        payload = {
            "content" : "new_content"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}",json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200


    def test_24_change_user_role_success(self):
        payload = {
            "login" : TestGeneral.user2_login,
            "role" : "reader"
        }

        response= requests.post(f"{BASE_URL}/api/v1/notes/roles/{TestGeneral.note_id}", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 201
        result = response.json()
        assert "message" in result and result["message"] == "Role added"
        assert "status" in result and result["status"] == "Success"
        assert "data" in result


    def test_25_change_user_role_error_editor_role(self):
        payload = {
            "login" : TestGeneral.user_login,
            "role" : "editor"
        }
        response = requests.post(f"{BASE_URL}/api/v1/notes/roles/{TestGeneral.note_id}", json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user2_access_token}"})
        assert response.status_code == 403
        result = response.json()
        assert "status" in result and result["status"] == "Error"
        assert "message" in result and result["message"] == "Access denied"

    def test_26_delete_note_by_id_error_someone_elses_token(self):
        response = requests.delete(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", headers={"Authorization" : f"Bearer {TestGeneral.user2_access_token}"})
        assert response.status_code == 403
        result = response.json()
        assert "status" in result and result["status"] == "Error"
        assert "message" in result and result["message"] == "Access denied"

    def test_27_delete_note_success(self):
        response= requests.delete(f"{BASE_URL}/api/v1/notes/{TestGeneral.note_id}", headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "message" in result and result["message"] == "Note deleted"
        assert "status" in result and result["status"] == "Success"

    
    def test_28_change_user_public_data_success(self):
        payload = {
            "phone" : "12345",
            "age" : 12,
            "username" : "aba"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/public",json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200
        result = response.json()
        assert "status" in result and result["status"] == "Success"
        assert "data" in result
        assert "message" in result and result["message"] == "User edited"


    def test_29_change_user_public_data_error_duplicate_phone(self):
        payload = {
            "phone" : "12345"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/public",json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user2_access_token}"})
        assert response.status_code == 409
        result = response.json()
        assert "message" in result  and result["message"] == "Duplicate key"


    def test_30_change_user_private_data_succes(self):
        payload = {
            "email" : "new_email",
            "password" : TestGeneral.user_password
        }
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/private",json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 200


    def test_30_change_user_private_data_error_duplicate_key(self):
        payload = {
            "email" : TestGeneral.user2_email,
            "password" : TestGeneral.user_password

        }
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/private",json=payload, headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 409


    def test_31_change_user_private_data_error_wrong_password(self):
        payload = {
            "email" : "new_email",
            "password" : "wrong_password"
        }
        response = requests.patch(f"{BASE_URL}/api/v1/users/me/private", json=payload,headers={"Authorization" : f"Bearer {TestGeneral.user_access_token}"})
        assert response.status_code == 403



    
        







        





    

    




    


    
        

        




