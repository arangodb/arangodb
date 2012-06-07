class VersionHandler < Arango::AbstractServlet

  def do_GET request, response
    response.status = 200
    response['Content-Type'] = 'text/plain'
    response.body = 'Hallo, World!'
  end

end

Arango::HttpServer.mount "/_ruby/version", VersionHandler
