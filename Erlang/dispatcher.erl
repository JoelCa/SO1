-module(dispatcher).
-compile(export_all).

inicio(Port,ListPids) ->
    {ok, ServerSocket} = gen_tcp:listen(Port,[{active,false}]),
    P1 = spawn (?MODULE, round, [ListPids, length(ListPids), 1, 0]),
    register(workerselector, P1),
    P2 = spawn (?MODULE, fdSelector, [1]),
    register(fdselector, P2),
    io:format("Servidor aceptando clientes\n"),
    serv(ServerSocket).

serv(ServerSocket) ->
    {ok, ClientSocket} = gen_tcp:accept(ServerSocket),
    P = spawn (?MODULE, proc_socket, [ClientSocket,{sinworker,-1},[]]),
    gen_tcp:controlling_process(ClientSocket, P),
    serv(ServerSocket).

proc_socket(ClientS,Tuple,List) ->
    case gen_tcp:recv(ClientS,0) of
        {ok, Bin} ->
            proc_socket(ClientS,Bin,Tuple,List);
        {error,closed} ->
            case Tuple of
                {sinworker,_} ->
                    ok;
                {_, ID} -> 
                    salir(List),
                    io:format("<- Cliente con ID ~p desconectado\n", [ID])
                    %% receive
                    %%     {numworker, N} ->
                    %%         io:format("Cliente desconectado: ID ~p\n", [N])
                    %% end
            end
    end.


proc_socket(ClientS,Bin,{sinworker,_},List) ->
    case string:sub_word(Bin,1) of
        "CON\r\n"->
            workerselector ! {getworker, self()},
            receive
                {worker, P, X} ->
                    gen_tcp:send(ClientS, lists:concat(["OK ID ",X, "\n"])),
                    io:format("-> Cliente con ID ~p conectado\n", [X]),
                    proc_socket(ClientS, {P,X}, List);
                {workererror} ->
                    gen_tcp:send(ClientS, "ERROR SERVIDOR SATURADO\n"), proc_socket(ClientS, sinworker, List)
            end;
        _ -> proc_socket(ClientS,{sinworker,-1},List)
    end;
proc_socket(ClientS,T,{W,ID},List) ->
    Tokens = string:tokens(T, " "),
    case lists:nth(1,Tokens) of
        "CON\r\n" ->
            gen_tcp:send(ClientS, "ERROR YA ESTA CONECTADO\n"), proc_socket(ClientS, {W,ID},List);
        "BYE\r\n" ->
            salir(List),
            gen_tcp:send(ClientS, "OK\n"),
            gen_tcp:close(ClientS),
            receive
                {numworker, N} ->
                    io:format("Cliente desconectado: worker nº ~p\n", [N])
            end;
        "CRE"  ->
             if
                length(Tokens) > 2  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {cre, X, ID, self()},
                            receive 
                                {cre, ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {cre, error} ->
                                    gen_tcp:send(ClientS, "ERROR EXISTE UN ARCHIVO CON ESE NOMBRE\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "LSD\r\n"->
            W ! {lsd, ID, self()},
            receive
                {lsd, X} ->
                    gen_tcp:send(ClientS, lists:concat(["OK",X]))
            after 100 ->
                    io:format("ERROR WORKER NO RESPONDE\n")
            end,
            proc_socket(ClientS, {W,ID}, List);
        "DEL"  ->
            if
                length(Tokens) > 2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {del, X, ID, self()},
                            receive
                                {del,ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {del,error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n");
                                {del,error2} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "OPN"  ->
            case analizar(Tokens,opn) of
                error1 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"), proc_socket(ClientS, {W,ID}, List);
                {M,X} ->
                    Y = lists:filter(fun({A,_,_,_}) -> A == X end, List),
                    case Y of
                        [] ->
                            W ! {opn, X, M, ID, self()},
                            receive
                                {opn, error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n"), proc_socket(ClientS, {W,ID}, List);
                                {opn, error2} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO EN MODO RDWR POR OTRO USUARIO\n"), proc_socket(ClientS, {W,ID}, List);
                                {opn, P} ->
                                    fdselector ! {newfd, self()},
                                    receive {fd, I} -> gen_tcp:send(ClientS, lists:concat(["OK FD ",I,"\n"])) end,
                                    proc_socket(ClientS, {W,ID}, [{X,M,I,P}|List])
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end;
                        _ ->
                            gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO\n"), proc_socket(ClientS, {W,ID}, List)
                    end
            end;
        "WRT"  ->
            case analizar(Tokens, wrt) of
                error1  ->
                    gen_tcp:send(ClientS, "ERROR TAMAÑO INCORRECTO\n");
                error2  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                {FD,Size,Buff} ->
                    X = lists:filter(fun({_,_,A,_}) -> A == FD end, List),
                    case X of
                        [] ->
                            gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n");
                        [{N,M,_,PidW}] ->
                            if
                                M == l ->
                                    gen_tcp:send(ClientS, "ERROR ARCHIVO ABIERTO EN MODO RDONLY\n");
                                true ->
                                    W ! {wrt,N,Size,Buff,PidW,self()},
                                    receive
                                        {wrt, ok} ->
                                            gen_tcp:send(ClientS,"OK\n")
                                    after 100 ->
                                            io:format("ERROR WORKER NO RESPONDE\n")
                                    end
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "REA"  ->
            case analizar(Tokens, rea) of
                error2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                {FD,Size} ->
                    %io:format("valor REA antes ~p, fd ~p",[List, FD]),
                    X = lists:filter(fun({_,_,A,_}) -> A == FD end, List),
                    %io:format("valor REA despues ~p",[X]),
                    case X of
                        [] ->
                            gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n");
                        [{N,_,_,PidW}] ->
                            W ! {rea,N,PidW,Size,self()},
                            receive
                                {rea,R,S} ->
                                    gen_tcp:send(ClientS, lists:concat(["OK SIZE ",S," ",R,"\n"]))
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "CLO" ->
            X = lists:nth(2,Tokens),
            if
                length(Tokens) /= 3  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"),
                    proc_socket(ClientS, {W,ID}, List);
                true ->
                    FD = fst(string:to_integer(lists:nth(3,Tokens))),
                    if
                        is_integer(FD) and (FD > 0) and (X == "FD") ->
                            Y = lists:filter(fun({_,_,A,_}) -> A == FD end, List),
                            case Y of
                                [] ->
                                    gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n"),
                                    proc_socket(ClientS, {W,ID}, List);
                                [{N,M,I,PidW}] ->
                                    W ! {clo, N, M, PidW, self()},
                                    receive
                                        {clo, ok} -> gen_tcp:send(ClientS, "OK\n")
                                    after 100 ->
                                            io:format("ERROR WORKER NO RESPONDE\n")
                                    end,
                                    proc_socket(ClientS, {W,ID}, List--[{N,M,I,PidW}])
                            end;
                        true  ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"),
                            proc_socket(ClientS, {W,ID}, List)
                    end
            end;
        "RM" ->
            if
                length(Tokens) > 2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {rm, X, ID, self()},
                            receive
                                {rm,ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {rm,error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n");
                                {rm,error2} ->
                                    gen_tcp:send(ClientS, "ERROR PEDIDO YA REALIZADO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        _  -> 
            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"), proc_socket(ClientS, {W,ID}, List)
    end.

                                                %round, mantiene una lista con los pids de los workers,
                                                %para asociarlos con los procesos socket. Además mantiene
                                                %un contador que será el ID del cliente.
                                                %Los workers se asignan en ronda.
round(L, N, I, ID) ->
    receive
        {getworker, Pid} ->
            Pid ! {worker, lists:nth(I, L), ID},
            round(L, N, (I rem N) + 1, ID +1)
    end.

fdSelector(N) ->
    receive
        {newfd, Pid} ->
            Pid ! {fd, N},
            fdSelector(N+1)
    end.


                                                %"analizar" determina los errores y el nombre del archivo, si existe,
                                                %para el pedido, dado en el 1º argumento, por los operadores WRT o REA.

analizar(L,opn) when length(L) /= 4 -> error1;
analizar([_,_,_,"\r\n"],opn) -> error1;
analizar([_,X,M,Y],opn) ->
    if 
        X == "MODE" ->
            case M of
                "RDWR" ->
                    {lye,Y};
                "RDONLY" ->
                    {l,Y};
                _ ->
                    error1
            end;
        true ->
            error1
    end;
analizar(T, rea) when length(T) /= 5 -> error2;
analizar([_,_,_,_,"\r\n"], rea) -> error2;
analizar([_,"FD", X, "SIZE", Y], rea) -> 
    FD = fst(string:to_integer(X)),
    Size = fst(string:to_integer((Y -- "\r\n"))),
    if
        is_integer(FD) and is_integer(Size) and (FD > 0) and (Size >= 0)->
            {FD,Size};
        true ->
            error2
    end;
analizar(_,rea) -> error2;
analizar(T, wrt) when length(T) < 5 -> error2;
analizar([_,_,_,_,_,"\r\n"], wrt) -> error2;
analizar([_,"FD", X, "SIZE",Y|Z], wrt) -> 
    FD = fst(string:to_integer(X)),
    Size = fst(string:to_integer(Y)),
    Buff = string:join(Z," ") -- "\r\n",
    if
        is_integer(FD) and is_integer(Size) and (FD > 0) and (Size >= 0)->
            if
                (Size > 0) and (Buff == []) ->
                    error2;
                Size == length(Buff) ->
                    {FD,Size,Buff};
                true ->
                    error1
            end;
        true ->
            error2
    end;
analizar(_,wrt) -> error2.


salir([]) ->
    ok;
salir([{N,M,_,P}|L]) ->
    P ! {clo,N,M,P, self()},
    salir(L).

fst({X,_}) -> X.

numero(P,L) -> numero(P,L,1).

numero(P,[{P,_}|_],N) ->
    N;
numero(P,[_|L],N) ->
    numero(P,L,N+1);
numero(_,[],_) ->
    error.
