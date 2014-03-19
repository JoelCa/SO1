-module(dispatcher).
-compile(export_all).

inicio(Port) ->
    {ok, ServerSocket} = gen_tcp:listen(Port,[{active,false}]),
    P1 = spawn (?MODULE, listaDeWorkers, [[{X,0} || X <- lists:seq(1,5)]]),
    register(listworker, P1),
    P2 = spawn(?MODULE, descriptor,[[],1]),
    register(desc, P2),
    serv(ServerSocket).

serv(ServerSocket) ->
    {ok, ClientSocket} = gen_tcp:accept(ServerSocket),
    PTemp = spawn(?MODULE, serv_temporizador,[ClientSocket,0]),
    P = spawn (?MODULE, loop, [ClientSocket,worker0,[], PTemp]),
    gen_tcp:controlling_process(ClientSocket, P),
    io:format("Un nuevo cliente\n"),
    serv(ServerSocket).

loop(ClientS,W,List,PTemp) ->
    case gen_tcp:recv(ClientS,0) of
        {ok, Bin} ->
            proc_socket(ClientS,Bin,W,List,PTemp);
        {error,closed} ->
            L = atom_to_list(W),
            X = fst(string:to_integer(string:sub_string(L,7,7))),
            case X of
                0 ->
                    ok;
                _ -> 
                    salir(List),
                    listworker ! {inactivo, L},
                    serv_temp_terminar(PTemp),
                    io:format("Cliente ~p desconectado\n", [X])
            end
    end.

proc_socket(ClientS,T,worker0,List,PTemp) ->
    case string:sub_word(T,1) of
        "CON\r\n"->
            iniciar_temp(PTemp),
            listworker ! {activo, self()},
            receive
                {numworker, X} ->
                    respuesta(ClientS, PTemp, lists:concat(["OK ID ",X, "\n"])),
                    io:format("Cliente ~p conectado\n", [X]),
                    loop(ClientS, nworker(X), List,PTemp);
                {numworkererror} ->
                    respuesta(ClientS, PTemp, "ERROR SERVIDOR SATURADO\n"), loop(ClientS, worker0,List,PTemp)
            end;
        _ -> loop(ClientS,worker0,List,PTemp)
    end;
proc_socket(ClientS,T,W,List,PTemp) ->
    iniciar_temp(PTemp),
    Tokens = string:tokens(T, " "),
    case lists:nth(1,Tokens) of
        "CON\r\n" ->
            respuesta(ClientS, PTemp, "ERROR YA ESTA CONECTADO\n"), loop(ClientS, W,List,PTemp);
        "BYE\r\n" ->
            L = atom_to_list(W),
            salir(List),
            respuesta(ClientS, PTemp, "OK\n"),
            gen_tcp:close(ClientS),
            listworker ! {inactivo, L},
            serv_temp_terminar(PTemp),
            io:format("Cliente ~p desconectado\n", [fst(string:to_integer(string:sub_string(L, 7, 7)))]);
        "CRE"  ->
             if
                length(Tokens) > 2  ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {cre, X, self()},
                            receive 
                                {cre, ok} ->
                                    respuesta(ClientS, PTemp, "OK\n");
                                {cre, error} ->
                                    respuesta(ClientS, PTemp, "ERROR EXISTE UN ARCHIVO CON ESE NOMBRE\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            loop(ClientS, W, List,PTemp);
        "LSD\r\n"->
            W ! {lsd, self()},
            receive
                {lsd, X} ->
                    respuesta(ClientS, PTemp, lists:concat(["OK",X]))
            after 100 ->
                    io:format("ERROR WORKER NO RESPONDE\n")
            end,
            loop(ClientS, W, List,PTemp);
        "DEL"  ->
            if
                length(Tokens) > 2 ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {del, X, self()},
                            receive
                                {del,ok} ->
                                    respuesta(ClientS, PTemp, "OK\n");
                                {del,error1} ->
                                    respuesta(ClientS, PTemp,"ERROR EL ARCHIVO NO EXISTE\n");
                                {del,error2} ->
                                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            loop(ClientS, W, List,PTemp);
        "OPN"  ->
            case analizar_opn(Tokens) of
                error1 ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n"), loop(ClientS,W,List,PTemp);
                {M,X} ->
                    Y = lists:filter(fun({A,_,_}) -> A == X end, List),
                    case Y of
                        [] ->
                            W ! {opn, X, M, self()},
                            receive
                                {opn, error1} ->
                                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO NO EXISTE\n"), loop(ClientS,W,List,PTemp);
                                {opn, error2} ->
                                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO EN MODO RDWR POR OTRO USUARIO\n"), loop(ClientS,W,List,PTemp);
                                {opn, P} ->
                                    desc ! {des, ins, X, P, self()},
                                    receive {des, I} -> respuesta(ClientS, PTemp, lists:concat(["OK FD ",I,"\n"])) end,
                                    loop(ClientS,W,[{X,M,P}|List],PTemp)
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end;
                        _ ->
                            respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO\n"), loop(ClientS,W,List,PTemp)
                    end
            end;
        "WRT"  ->
            case analizar(Tokens, wrt) of
                error1  ->
                    respuesta(ClientS, PTemp, "ERROR TAMAÑO INCORRECTO\n");
                error2  ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                error3 ->
                    respuesta(ClientS, PTemp, "ERROR FD INCORRECTO\n");
                error4 ->
                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n");
                {N,Size,P,S} ->
                    {_,M,_} = lists:nth(1,lists:filter(fun({A,_,_}) -> A == N end, List)),
                    if
                        M == l ->
                            respuesta(ClientS, PTemp, "ERROR ARCHIVO ABIERTO EN MODO RDONLY\n");
                        true ->
                            W ! {wrt,N,Size,S,P,self()},
                            receive
                                {wrt, ok} ->
                                    respuesta(ClientS, PTemp,"OK\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            loop(ClientS, W, List,PTemp);
        "REA"  ->
            case analizar(Tokens, rea) of
                error2 ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                error3 ->
                    respuesta(ClientS, PTemp, "ERROR FD INCORRECTO\n");
                error4 ->
                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n");
                {N,P,Size} ->
                    W ! {rea,N,P,Size,self()},
                    receive
                        {rea,R,S} ->
                            respuesta(ClientS, PTemp, lists:concat(["OK SIZE ",S," ",R,"\n"]))
                    after 100 ->
                            io:format("ERROR WORKER NO RESPONDE\n")
                    end
            end,
            loop(ClientS, W, List, PTemp);
        "CLO" ->
            X = lists:nth(2,Tokens),
            if
                length(Tokens) /= 3  ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n"),
                    loop(ClientS,W,List,PTemp);
                true ->
                    FD = fst(string:to_integer(lists:nth(3,Tokens))),
                    if
                        is_integer(FD) and (FD > 0) and (X == "FD") ->
                            case nd(FD,1) of
                                error3 ->
                                    respuesta(ClientS, PTemp, "ERROR FD INCORRECTO\n"),
                                    loop(ClientS,W,List,PTemp);
                                error4 ->
                                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n"),
                                    loop(ClientS,W,List,PTemp);
                                {N,P} ->
                                    [{A,M,B}] = lists:filter(fun({A,_,_}) -> A == N end, List),
                                    W ! {clo, N, M, P, self()},
                                    receive
                                        {clo, ok} -> respuesta(ClientS, PTemp, "OK\n")
                                    after 100 ->
                                            io:format("ERROR WORKER NO RESPONDE\n")
                                    end,
                                    loop(ClientS,W,List--[{A,M,B}],PTemp)
                            end;
                        true  ->
                            respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n"),
                            loop(ClientS,W,List,PTemp)
                    end
            end;
        "RM" ->
            if
                length(Tokens) > 2 ->
                    respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {rm, X, self()},
                            receive
                                {rm,ok} ->
                                    respuesta(ClientS, PTemp, "OK\n");
                                {rm,error1} ->
                                    respuesta(ClientS, PTemp, "ERROR EL ARCHIVO NO EXISTE\n");
                                {rm,error2} ->
                                    respuesta(ClientS, PTemp, "ERROR PEDIDO YA REALIZADO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            loop(ClientS, W, List,PTemp);
        _  -> 
            respuesta(ClientS, PTemp, "ERROR DE SINTAXIS\n"), loop(ClientS, W, List,PTemp
)
    end.

                                                %"listaDeWorkers" tiene un Buffer de tuplas de la forma
                                                %{1,0},{2,0},...{5,0}. 
                                                %Si la segunda componente de la i-esima tupla es 1 el
                                                %worker numero "i" esta activo, si es 0 esta inactivo.
                                                %Me sirve para determinar que worker esta libre, para
                                                %poder asignarlo a un nuevo cliente.
listaDeWorkers(L) ->
    receive
        {activo, Pid} ->
            L2 = lists:filter(fun({_,Y}) -> Y==0 end, L), T = length(L2),
            if
                T>0 ->
                    {A,_} = lists:nth(random:uniform(T), L2),
                    Pid ! {numworker, A}, listaDeWorkers(lists:map(fun({X,Y}) -> if X==A -> {X,1}; true -> {X,Y} end end, L));
                true ->
                    Pid ! {numworkererror}, listaDeWorkers(L)
            end;
        {inactivo, A} ->
            B = fst(string:to_integer(string:sub_string(A, 7, 7))),
            listaDeWorkers(lists:map(fun({X,Y}) -> if X==B -> {X,0}; true -> {X,Y} end end, L))
    end.

borrarDes(N,P,[{N,[{P,_}],_}|L]) ->
    L;
borrarDes(N,P,[{N,A,B}|L]) ->
    [{N,lists:filter(fun({C,_}) -> P /= C end, A),B}|L];
borrarDes(N,P,[A|L]) ->
    [A|borrarDes(N,P,L)].


                                                %El arg. Buff es una lista de tuplas con el siguiente formato:
                                                %-N corresponde al nombre del archivo.
                                                %-I es una lista de tuplas con los pids del que abrio el archivo su FD asociado.
                                                %-P1 es el Pid del worker que tiene el archivo.
                                                %en el 3º caso del receive, "Flag" es una bandera
                                                %para identificar si es invocada por el operador CLO.
                                                %Me sirve para conocer los archivos arbiertos
                                                %por todos los clientes.
descriptor(Buff, I) ->
    receive 
        {des, ins, N, P, Pid} ->
            Pid ! {des, I},
            X  = lists:filter(fun({A,_,_}) -> A == N end, Buff),
            case X of
                [] ->
                    descriptor([{N,[{Pid,I}],P}|Buff],I+1);
                _ ->
                    descriptor(lists:map(fun({A,B,C}) -> if A == N -> {A,[{Pid,I}|B],C}; true -> {A,B,C} end end, Buff), I+1)
            end;
        {des, borrar, N, Pid} ->
            X = borrarDes(N,Pid,Buff),
            Pid ! {des,ok},
            descriptor(X,I);
        {des, Flag, FD, Pid} ->
            X = lists:filter(fun({_,A,_}) -> B = lists:filter(fun({_,D}) -> D == FD end, A), if length(B)>0 -> true; true -> false end end, Buff),
            case X of
                []  ->
                    Pid ! {des, error1}, descriptor(Buff,I);
                [{Y,Z,W}] ->
                    [{Z2,_}] = lists:filter(fun({_,A}) -> A == FD end, Z),
                    if
                        Pid ==   Z2 ->
                            Pid ! {des, Y, W},
                            if
                                Flag == 1 ->
                                    if
                                        length(Z) == 1 ->
                                            descriptor(Buff -- [{Y,Z,W}],I);
                                        true ->
                                            descriptor(borrarDes(Y,Z2,Buff), I)
                                    end;
                                true ->
                                    descriptor(Buff,I)
                            end;
                        true ->
                            Pid ! {des, error2}, descriptor(Buff,I)
                    end
            end
    end.

analizar_opn(L) when length(L) /= 4 -> error1;
analizar_opn([_,_,_,"\r\n"]) -> error1;
analizar_opn([_,X,M,Y]) ->
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
    end.

                                                %"analizar" determina los posibles errores y el nombre del archivo, si existe,
                                                %para el pedido dado en el 1º argumento, por los operadores WRT o REA.


analizar(T, rea) when length(T) /= 5 -> error2;
analizar([_,_,_,_,"\r\n"], rea) -> error2;
analizar([_,"FD", X, "SIZE", Y], rea) -> 
    FD = fst(string:to_integer(X)),
    Size = fst(string:to_integer((Y -- "\r\n"))),
    if
        is_integer(FD) and is_integer(Size) and (FD > 0) and (Size >= 0)->
            case nd(FD,0) of
                {N,P} ->
                    {N,P,Size};
                E ->
                    E
            end;
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
                    case nd(FD,0) of
                        {N,P} ->
                            {N,Size,P,Buff};
                        E ->
                            E
                    end;
                true ->
                    error1
            end;
        true ->
            error2
    end;
analizar(_,wrt) -> error2.

                                                %"nd" obtiene el nombre y pid asociado al descriptor FD.
                                                %Invicada por "analizar", y por el op. CLO.
nd(FD,Flag) ->
    desc ! {des, Flag, FD, self()},
    receive
        {des, error1}->
            error3;
        {des, error2} ->
            error4;
        {des, N, P}->
            {N,P}
    end.

salir([]) ->
    ok;
salir([{N,M,P}|L]) ->
    P ! {clo,N,M,P, self()},
    receive {clo,ok} ->
            desc ! {des,borrar,N,self()},
            receive {des, ok} -> salir(L) end
    end.

nworker(N) ->
    X = lists:concat(["worker",N]),
    list_to_atom(X).

fst({X,_}) -> X.


serv_temp_terminar(Pid) ->
    Pid ! {cerrar,self()},
    receive
        cerrado ->
            ok
    after 100 ->
            io:format("ERROR TEMPORIZADOR\n")
    end.

iniciar_temp(Pid) ->
    Pid ! {iniciar,self()},
    receive
        aceptado ->
            ok
    after 100 ->
            io:format("ERROR TEMPORIZADOR\n")
    end.

finalizar_temp(Pid) ->
    Pid ! {anular,self()},
    receive
        anulado ->
            ok;
        errorcontador ->
            error
    after 100 ->
            io:format("ERROR TEMPORIZADOR\n")
    end.

serv_temporizador(ClientS,0) ->
    receive
        {iniciar, Pid} ->
            Pid ! aceptado,
            serv_temporizador(ClientS,1);
        {anular, Pid} ->
            Pid ! errorcontador,
            serv_temporizador(ClientS,0);
        {cerrar,Pid} ->
            Pid ! cerrado
    end;
serv_temporizador(ClientS,1) ->
    receive
        {anular, Pid} ->
            Pid ! anulado
    after 100 ->
            gen_tcp:send(ClientS, "ERROR 62 ETIME\n")
    end,
    serv_temporizador(ClientS,0).

respuesta(ClientS, Pid, Resp) ->
    case finalizar_temp(Pid) of
        ok ->
            gen_tcp:send(ClientS, Resp);
        error ->
            io:format("ERROR TEMPORIZADOR AGOTADO\n")
    end.
