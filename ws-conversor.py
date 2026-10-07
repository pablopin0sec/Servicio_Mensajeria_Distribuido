import re
from spyne import Application, ServiceBase, Integer, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication


class Conversor(ServiceBase):
    @rpc(Unicode, _returns=Unicode)
    def normalizar(ctx, mensaje):
        return re.sub(r' +', ' ', mensaje).strip()

application = Application(
    services=[Conversor],
    tns='http://conversor.uc3m.es/',
    in_protocol=Soap11(validator='lxml'),
    out_protocol=Soap11()
)
application = WsgiApplication(application)

if __name__ == '__main__':
    from wsgiref.simple_server import make_server

    server = make_server('127.0.0.1', 8000, application)
    server.serve_forever()