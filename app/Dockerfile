FROM drogonframework/drogon:latest

WORKDIR /app
RUN /bin/sh -c "drogon_ctl create project weather_backend"

WORKDIR /app/weather_backend
RUN rm -f main.cc
COPY ./src/main.cc ./main.cc

WORKDIR /app/weather_backend/build

RUN cmake .. && make

ENTRYPOINT [ "./weather_backend" ]