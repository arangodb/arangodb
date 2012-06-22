class VersionHandler < Arango::AbstractServlet

  def do_GET request, response
    response.status = 200
    response.content_type = 'text/plain'
    response.body = 'Hallo, World!'
  end

end

Arango::HttpServer.mount "/_admin/ruby-version", VersionHandler
