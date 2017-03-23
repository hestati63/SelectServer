# Select Server

## Usage
```c
bool handler(SelectCTX *ctx)
{
    write(1, ctx->buffer, ctx->sz);
    return true; /* on success */
}

int main()
{
    Server *server = newServer(1234, handler);
    ServerLoop(server);
}
```
