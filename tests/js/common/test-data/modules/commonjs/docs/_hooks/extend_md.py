import markdown

MARKDOWN_EXTENSIONS = ["def_list", "fenced_code", "codehilite", "tables"]

def extended_markdown(text):
    if isinstance(text, str):
        text = text.decode("utf8")
    return markdown.markdown(text, extensions=MARKDOWN_EXTENSIONS, 
        output_format="html")
    
Config.transformers['markdown'] = extended_markdown

