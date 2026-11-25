from http.server import BaseHTTPRequestHandler, HTTPServer
import base64, json
class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        assert self.path == '/ords/demo/_/sql', self.path
        if self.headers.get('Authorization') != 'Basic ' + base64.b64encode(b'demo:secret').decode():
            self.send_response(401); self.end_headers(); return
        request = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        sql = request['statementText']
        binds = request.get('binds', [])
        if 'BROKEN' in sql:
            encoded = json.dumps({'error': {'message': 'ORA-00942: table or view does not exist'}}).encode()
            self.send_response(555); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(encoded))); self.end_headers(); self.wfile.write(encoded); return
        if 'PAGINATION_TEST' in sql:
            assert request.get('offset') == 5 and request.get('limit') == 2, request
            body = {'items': [{'statementType': 'query', 'result': 0, 'resultSet': {'metadata': [], 'items': [{'id': 1}, {'id': 2}], 'count': 2, 'offset': 5, 'limit': 2, 'hasMore': True}}]}
        elif 'OUT_BIND_TEST' in sql:
            body = {'items': [{'statementType': 'plsql', 'result': 0, 'binds': [{'name': 'out_id', 'data_type': 'NUMBER', 'mode': 'out', 'result': 9876}]}]}
        elif 'FROM dual' in sql:
            if ':id AS ID' in sql:
                by_name = {item.get('name'): item for item in binds}
                assert by_name['id']['value'] == 42 and by_name['name']['value'] == "O'Reilly" and by_name['rate']['value'] == '12.5' and by_name['active']['value'] is True and by_name['note']['value'] is None, request
            if 'SELECT ? AS ID' in sql:
                assert binds == [{'index': 1, 'data_type': 'NUMBER', 'value': 42}], request
            body = {'items': [{'statementType': 'query', 'result': 0, 'resultSet': {'metadata': [{'columnName': 'ID', 'jsonColumnName': 'id', 'columnTypeName': 'NUMBER', 'precision': 10, 'scale': 0}, {'columnName': 'NAME', 'jsonColumnName': 'name', 'columnTypeName': 'VARCHAR2'}, {'columnName': 'RATE', 'jsonColumnName': 'rate', 'columnTypeName': 'NUMBER', 'precision': 8, 'scale': 2}, {'columnName': 'ACTIVE', 'jsonColumnName': 'active', 'columnTypeName': 'BOOLEAN'}, {'columnName': 'NOTE', 'jsonColumnName': 'note', 'columnTypeName': 'VARCHAR2'}], 'items': [{'id': 42, 'name': "O'Reilly", 'rate': 12.5, 'active': True, 'note': None}], 'count': 1, 'hasMore': False}}]}
        else:
            body = {'items': [{'statementType': 'dml', 'result': 1, 'response': ['1 row inserted.']}]}
        encoded = json.dumps(body).encode()
        self.send_response(200); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(encoded))); self.end_headers(); self.wfile.write(encoded)
    def log_message(self, *args): pass
HTTPServer(('127.0.0.1', 18080), Handler).serve_forever()
