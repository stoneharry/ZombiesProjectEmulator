using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MySql.Data;
using MySql.Data.MySqlClient;
using System.Data;
using System.Reflection;
using System.Collections;
using System.Runtime.CompilerServices;

namespace ItemDBCGenerator
{
    class MySQL
    {
        private Config config;
        private MySqlConnection conn;
        public String Table;

        public MySQL(Config config)
        {
            this.config = config;
            this.Table = config.Table;

            String connectionString = "server={0};port={1};uid={2};pwd={3};";
            connectionString = String.Format(connectionString,
                config.Host, config.Port, config.User, config.Pass);

            conn = new MySqlConnection();
            conn.ConnectionString = connectionString;
            conn.Open();
            // Create DB
            var cmd = conn.CreateCommand();
            cmd.CommandText = String.Format("USE `{0}`;", config.Database);
            cmd.ExecuteNonQuery();
            cmd.Dispose();
        }

        ~MySQL()
        {
            if (conn != null)
                conn.Close();
        }

        private readonly object syncLock = new object();

        public DataTable query(String query)
        {
            lock (syncLock)
            {
                var adapter = new MySqlDataAdapter(query, conn);
                DataSet DS = new DataSet();
                adapter.SelectCommand.CommandTimeout = 0;
                adapter.Fill(DS);
                return DS.Tables[0];
            }
        }

        public void execute(string p)
        {
            //lock (syncLock)
            //{
            var cmd = conn.CreateCommand();
            cmd.CommandText = p;
            cmd.ExecuteNonQuery();
            cmd.Dispose();
            //}
        }
    }
}
